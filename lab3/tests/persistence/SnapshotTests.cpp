#include "../StoreFixtures.hpp"
#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"
#include "SnapshotStore.hpp"
#include "shop/application/ReturnProcessing.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/PaymentRuleException.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unistd.h>

using namespace shop;
using snapshot::Json;
namespace {
void verify(const StoreState& state) {
    const auto document = SnapshotCodec::encode(state);
    const auto restored = SnapshotCodec::decode(document);
    REQUIRE(restored == state);
    REQUIRE(SnapshotCodec::encode(restored) == document);
}
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory() {
        auto pattern =
            (std::filesystem::temp_directory_path() / "shop-snapshot-test-XXXXXX").string();
        const auto result = ::mkdtemp(pattern.data());
        if (!result) {
            throw std::runtime_error("mkdtemp failed");
        }
        path = result;
    }
    ~TemporaryDirectory() {
        // This directory and its immediate files are owned only by this test.
        std::error_code ignored;
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::filesystem::remove(entry.path(), ignored);
        }
        std::filesystem::remove(path, ignored);
    }
};
} // namespace

TEST_CASE("Full JSON snapshot round trips every purchase phase and continues idempotent payment "
          "and return") {
    auto state = test::store(true);
    verify(state);
    const auto quote = Checkout(state).preview("alice", test::address(), "standard");
    const auto id = Checkout(state).placeOrder("alice", quote, "place");
    verify(state);
    auto restored = SnapshotCodec::decode(SnapshotCodec::encode(state));
    REQUIRE(Checkout(restored).placeOrder("alice", quote, "place") == id);
    OrderProcessing process(restored);
    process.extendPaymentDeadline("alice", id, "extend");
    verify(restored);
    const auto terms = process.previewUnpaidDelivery("alice", id, test::address(), "express");
    process.changeUnpaidDelivery("alice", id, terms, "change");
    verify(restored);
    process.pay("alice", id, Money(120), PaymentMethod::SimulatedCard, false, "Отказ", "decline");
    verify(restored);
    process.pay("alice", id, Money(120), PaymentMethod::SimulatedTransfer, true, "", "pay");
    verify(restored);
    restored.advanceClock(50);
    verify(restored);
    auto continuation = SnapshotCodec::decode(SnapshotCodec::encode(restored));
    const auto prior = continuation;
    REQUIRE(OrderProcessing(continuation)
                .pay("alice", id, Money(120), PaymentMethod::SimulatedTransfer, true, "", "pay")
                .successful);
    REQUIRE(continuation == prior);
    test::ship(continuation, id);
    verify(continuation);
    OrderProcessing(continuation).confirmDelivery("alice", id, "deliver");
    verify(continuation);
    const auto review = continuation.addReview("alice", id, "shirt", 5, "Хороший товар");
    continuation.moderateReview(review, true);
    continuation.voteHelpful("bob", review);
    verify(continuation);
    ReturnProcessing returns(continuation);
    const auto request =
        returns.open("alice", id, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open");
    verify(continuation);
    returns.inspect(request, {InspectionResult("shirt-blue", 1, 1, 1, ItemCondition::Opened)},
                    "inspect");
    verify(continuation);
    auto final = SnapshotCodec::decode(SnapshotCodec::encode(continuation));
    REQUIRE(ReturnProcessing(final).complete(request, "complete") == Money(34));
    verify(final);
    const auto completed = final;
    REQUIRE(ReturnProcessing(final).complete(request, "complete") == Money(34));
    REQUIRE(final == completed);
}

TEST_CASE("JSON rejects duplicate keys unsupported versions types links cached totals and broken "
          "invariants") {
    auto state = test::store(true);
    const auto id = test::place(state);
    test::pay(state, id);
    test::ship(state, id);
    OrderProcessing(state).confirmDelivery("alice", id, "deliver");
    const auto valid = Json::parse(SnapshotCodec::encode(state));
    const auto reject = [&](const Json& data) {
        REQUIRE_THROWS_AS(SnapshotCodec::decode(data.dump()), SnapshotException);
    };
    auto data = valid;
    data["version"] = 2;
    reject(data);
    data = valid;
    data["clock"] = 10.0;
    reject(data);
    data = valid;
    data["clock"] = true;
    reject(data);
    data = valid;
    data["extra"] = 1;
    reject(data);
    data = valid;
    data["next_sequence"] = 1;
    reject(data);
    data = valid;
    data["inventory"]["items"][0]["reserved"] = 99;
    reject(data);
    data = valid;
    data["inventory"]["items"][0]["on_hand"] = -1;
    reject(data);
    data = valid;
    data["orders"]["entries"][0]["customer_id"] = "missing";
    reject(data);
    data = valid;
    data["orders"]["entries"][0]["lines"][0]["net_total"] = 999;
    reject(data);
    data = valid;
    data["orders"]["entries"][0]["status"] = 99;
    reject(data);
    data = valid;
    data["customers"][0]["loyalty"]["qualifying_spend"] = 999;
    reject(data);
    data = valid;
    data["payments"]["payments"][0]["captured_amount"] = 999;
    reject(data);
    data = valid;
    data["operations"][0]["signature"]["actor"] = "missing";
    reject(data);
    data = valid;
    data["catalog"]["categories"].push_back(data["catalog"]["categories"][0]);
    reject(data);
    REQUIRE_THROWS_AS(SnapshotCodec::decode("{\"format\":\"a\",\"format\":\"b\"}"),
                      SnapshotException);
    REQUIRE_THROWS_AS(SnapshotCodec::decode("{} garbage"), SnapshotException);
    REQUIRE_THROWS_AS(SnapshotCodec::decode(""), SnapshotException);
}

TEST_CASE("File snapshots replace atomically and failed loads or renames retain previous state and "
          "files") {
    TemporaryDirectory directory;
    const auto file = directory.path / "store.json";
    auto state = test::store();
    SnapshotStore::save(file, state);
    auto restored = test::store(true);
    SnapshotStore::load(file, restored);
    REQUIRE(restored == state);
    const StoreState orphaned(state.inventory(), state.deliveryPolicy(), state.now());
    REQUIRE_THROWS_AS(SnapshotStore::save(file, orphaned), SnapshotException);
    SnapshotStore::load(file, restored);
    REQUIRE(restored == state);
    const auto id = test::place(state);
    SnapshotStore::save(file, state);
    SnapshotStore::load(file, restored);
    REQUIRE(restored == state);
    const auto before = restored;
    {
        std::ofstream broken(file);
        broken << "{broken";
    }
    REQUIRE_THROWS_AS(SnapshotStore::load(file, restored), SnapshotException);
    REQUIRE(restored == before);
    REQUIRE_THROWS_AS(SnapshotStore::load(directory.path / "missing.json", restored),
                      SnapshotException);
    REQUIRE(restored == before);
    REQUIRE_THROWS_AS(SnapshotStore::save(directory.path, state), SnapshotException);
    REQUIRE_THROWS_AS(SnapshotStore::save(directory.path / "missing" / "store.json", state),
                      SnapshotException);
    REQUIRE_THROWS_AS(SnapshotStore::save({}, state), SnapshotException);
    std::size_t files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path)) {
        ++files;
        REQUIRE(entry.path().filename() == "store.json");
    }
    REQUIRE(files == 1);
}

TEST_CASE(
    "Snapshots preserve cancelled expired withdrawn rejected zero-money and moderated histories") {
    SECTION("Unpaid cancellation") {
        auto state = test::store(true);
        const auto id = test::place(state);
        OrderProcessing(state).cancel("alice", id, "cancel");
        verify(state);
    }
    SECTION("Paid packed cancellation") {
        auto state = test::store(true);
        const auto id = test::place(state);
        test::pay(state, id);
        OrderProcessing process(state);
        process.startPicking(id, "start");
        process.pick(id, "shirt-blue", 3, "pick");
        process.pack(id, "pack");
        verify(state);
        process.reopenPicking(id, "reopen");
        process.undoPick(id, "shirt-blue", 1, "undo");
        process.cancel("alice", id, "cancel");
        verify(state);
    }
    SECTION("Unpaid expiry") {
        auto state = test::store(true);
        test::place(state);
        state.advanceClock(30);
        Checkout(state).expireUnpaidOrders();
        verify(state);
    }
    SECTION("Return withdrawal rejection and amendments") {
        auto state = test::store();
        const auto id = test::place(state);
        test::pay(state, id);
        test::ship(state, id);
        OrderProcessing(state).confirmDelivery("alice", id, "deliver");
        ReturnProcessing process(state);
        auto request =
            process.open("alice", id, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open");
        process.amendSubmitted("alice", request, {{"shirt-blue", 2, ReturnReason::ChangedMind}},
                               "amend");
        verify(state);
        process.withdraw("alice", request, "withdraw");
        verify(state);
        request = process.open("alice", id, {{"shirt-blue", 1, ReturnReason::Defective}}, "open2");
        process.reject(request, "Причина", "reject");
        verify(state);
        request =
            process.open("alice", id, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open3");
        process.inspect(request,
                        {InspectionResult("shirt-blue", 1, 0, 0, ItemCondition::Incomplete)},
                        "inspect3");
        verify(state);
        request =
            process.open("alice", id, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open4");
        state.advanceClock(100);
        process.closeExpiredUnreceived();
        verify(state);
    }
    SECTION("Zero money return and review moderation") {
        auto state = test::store();
        const Promotion full("full", "full", PromotionKind::Percentage, 10000, Money{},
                             TimeWindow(0, 100), {}, false);
        state.addPromotion(full);
        state.registerCoupon(Coupon("FULL", "full", 1, 1, TimeWindow(0, 100)));
        auto cart = state.cart("alice");
        cart.setCoupon("FULL");
        state.replaceCart(cart);
        const auto id = test::place(state);
        test::pay(state, id);
        test::ship(state, id);
        OrderProcessing(state).confirmDelivery("alice", id, "deliver");
        const auto review = state.addReview("alice", id, "shirt", 5, "Отзыв");
        verify(state);
        state.moderateReview(review, true);
        state.voteHelpful("bob", review);
        state.moderateReview(review, false, "Проверка");
        verify(state);
        ReturnProcessing process(state);
        const auto request =
            process.open("alice", id, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open");
        process.inspect(request, {InspectionResult("shirt-blue", 1, 1, 1, ItemCondition::Opened)},
                        "inspect");
        process.complete(request, "complete");
        verify(state);
    }
}

TEST_CASE("Snapshots retain unavailable wishes arbitrary entered coupon codes and historical "
          "catalog names") {
    auto state = test::store();
    auto wishlist = state.wishlist("alice");
    wishlist.addProduct(state.catalog().product("shirt"));
    state.replaceWishlist(wishlist);
    auto book = state.addresses("alice");
    book.add("home", test::address());
    state.replaceAddresses(book);
    auto customer = state.customer("bob");
    customer.restrictPurchases("Проверка");
    state.replaceCustomer(customer);
    auto catalog = state.catalog();
    catalog.archiveProduct("shirt");
    catalog.archiveCategory("clothes");
    catalog.addCategory(Category("new-clothes", "Одежда", "root"));
    state.replaceCatalog(catalog);
    auto cart = state.cart("alice");
    cart.setCoupon("ENTERED-BUT-UNKNOWN");
    state.replaceCart(cart);
    verify(state);
}

TEST_CASE("JSON cannot inject orphan money returns or swap shipment-history references") {
    auto state = test::store();
    const auto id = test::place(state);
    test::pay(state, id);
    test::ship(state, id);
    OrderProcessing(state).confirmDelivery("alice", id, "deliver");
    auto data = Json::parse(SnapshotCodec::encode(state));
    data["orders"]["entries"][0]["history"][3]["related_entity_id"] = "other-shipment";
    REQUIRE_THROWS_AS(SnapshotCodec::decode(data.dump()), SnapshotException);
    data = Json::parse(SnapshotCodec::encode(state));
    const auto paymentId = data["payments"]["payments"][0]["id"].get<std::string>();
    data["payments"]["refunds"].push_back({{"id", "refund-100"},
                                           {"operation_id", "orphan-refund"},
                                           {"payment_id", paymentId},
                                           {"order_id", id},
                                           {"return_request_id", nullptr},
                                           {"amount", 1},
                                           {"created_at", 10}});
    data["payments"]["payments"][0]["refund_ids"].push_back("refund-100");
    data["payments"]["operations"].push_back({{"id", "orphan-refund"},
                                              {"operation",
                                               {{"kind", 1},
                                                {"payment_id", paymentId},
                                                {"return_request_id", nullptr},
                                                {"amount", 1},
                                                {"result_id", "refund-100"}}}});
    data["next_sequence"] = 101;
    REQUIRE_THROWS_AS(SnapshotCodec::decode(data.dump()), SnapshotException);
}

TEST_CASE(
    "Version one snapshots retain colliding user keys while restored orders and returns expire") {
    auto state = test::store();
    const auto quote = Checkout(state).preview("alice", test::address(), "standard");
    const auto orderId = Checkout(state).placeOrder("alice", quote, "system-expire-order-1");
    const auto held = SnapshotCodec::encode(state);
    REQUIRE(Json::parse(held).at("version") == 1);
    auto restored = SnapshotCodec::decode(held);
    restored.advanceClock(30);
    REQUIRE(Checkout(restored).expireUnpaidOrders() == std::vector<std::string>{orderId});
    REQUIRE(restored.operations().at("system-expire-order-1").signature.kind ==
            CommandKind::PlaceOrder);
    verify(restored);
    auto expired = SnapshotCodec::decode(SnapshotCodec::encode(restored));
    const auto before = expired;
    REQUIRE(Checkout(expired).expireUnpaidOrders().empty());
    REQUIRE(Checkout(expired).placeOrder("alice", quote, "system-expire-order-1") == orderId);
    REQUIRE(expired == before);

    test::pay(state, orderId);
    test::ship(state, orderId);
    OrderProcessing(state).confirmDelivery("alice", orderId, "deliver");
    ReturnProcessing returns(state);
    const std::vector<ReturnSelection> selection{{"shirt-blue", 1, ReturnReason::ChangedMind}};
    const std::string operation = "system-expire-return-return-6";
    const auto requestId = returns.open("alice", orderId, selection, operation);
    restored = SnapshotCodec::decode(SnapshotCodec::encode(state));
    restored.advanceClock(100);
    REQUIRE(ReturnProcessing(restored).closeExpiredUnreceived() ==
            std::vector<std::string>{requestId});
    REQUIRE(restored.operations().at(operation).signature.kind == CommandKind::OpenReturn);
    verify(restored);
    expired = SnapshotCodec::decode(SnapshotCodec::encode(restored));
    const auto closed = expired;
    REQUIRE(ReturnProcessing(expired).closeExpiredUnreceived().empty());
    REQUIRE(ReturnProcessing(expired).open("alice", orderId, selection, operation) == requestId);
    REQUIRE(expired == closed);
}

TEST_CASE(
    "Failed payment and inspection preserve every serialized field and permit valid retries") {
    auto state = test::store();
    const auto orderId = test::place(state);
    const auto beforePayment = SnapshotCodec::encode(state);
    REQUIRE_THROWS_AS(OrderProcessing(state).pay("alice", orderId, Money(1),
                                                 PaymentMethod::SimulatedCard, true, "", "pay"),
                      PaymentRuleException);
    REQUIRE(SnapshotCodec::encode(state) == beforePayment);
    state = SnapshotCodec::decode(beforePayment);
    test::pay(state, orderId);
    test::ship(state, orderId);
    OrderProcessing(state).confirmDelivery("alice", orderId, "deliver");
    const auto requestId = ReturnProcessing(state).open(
        "alice", orderId, {{"shirt-blue", 2, ReturnReason::ChangedMind}}, "open");
    const auto beforeInspection = SnapshotCodec::encode(state);
    REQUIRE_THROWS_AS(
        ReturnProcessing(state).inspect(
            requestId, {InspectionResult("shirt-blue", 1, 1, 1, ItemCondition::Opened)}, "inspect"),
        ReturnRuleException);
    REQUIRE(SnapshotCodec::encode(state) == beforeInspection);
    state = SnapshotCodec::decode(beforeInspection);
    REQUIRE(ReturnProcessing(state).inspect(
                requestId, {InspectionResult("shirt-blue", 2, 2, 2, ItemCondition::Opened)},
                "inspect") == "inspection-7");
    verify(state);
}

TEST_CASE("Identifier overflow discards a partially allocated checkout or payment sequence") {
    auto state = test::store();
    auto data = Json::parse(SnapshotCodec::encode(state));
    data["next_sequence"] = std::numeric_limits<std::int64_t>::max() - 1;
    state = SnapshotCodec::decode(data.dump());
    const auto quote = Checkout(state).preview("alice", test::address(), "standard");
    const auto beforeCheckout = state;
    REQUIRE_THROWS_AS(Checkout(state).placeOrder("alice", quote, "place"),
                      InvalidDomainDataException);
    REQUIRE(state == beforeCheckout);
    REQUIRE(SnapshotCodec::encode(state) == SnapshotCodec::encode(beforeCheckout));

    state = test::store();
    const auto orderId = test::place(state);
    data = Json::parse(SnapshotCodec::encode(state));
    data["next_sequence"] = std::numeric_limits<std::int64_t>::max() - 1;
    state = SnapshotCodec::decode(data.dump());
    const auto beforePayment = state;
    REQUIRE_THROWS_AS(OrderProcessing(state).pay("alice", orderId, Money(110),
                                                 PaymentMethod::SimulatedCard, true, "", "pay"),
                      InvalidDomainDataException);
    REQUIRE(state == beforePayment);
    verify(state);
}
