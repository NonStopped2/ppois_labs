#include "../Fixtures.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
namespace {
Parcel openParcel(const DeliveryOptions& options = DeliveryOptions{},
                  PackagingKind packaging = PackagingKind::Box) {
    return Parcel(test::customer(), test::customer("other"), 500, options, Dimensions(200, 100, 50),
                  packaging);
}
ContentItem item(std::int64_t quantity = 1, std::int64_t weight = 100) {
    return ContentItem("Книга", quantity, weight, Money(100), ContentCategory::GeneralGoods);
}
} // namespace
TEST_CASE("Parcel keeps inventory, packaging and seal as owned values", "[parcel]") {
    auto parcel = openParcel();
    REQUIRE(parcel.kind() == ShipmentKind::Parcel);
    REQUIRE(parcel.packagingKind() == PackagingKind::Box);
    REQUIRE(parcel.sealId().empty());
    REQUIRE_THROWS_AS(parcel.seal("seal"), InvalidShipmentStateException);
    parcel.addItem(item(2));
    REQUIRE(parcel.contents().size() == 1);
    REQUIRE(parcel.revision() == 1);
    REQUIRE_THROWS_AS(parcel.seal("bad seal"), std::invalid_argument);
    REQUIRE(parcel.sealId().empty());
    parcel.seal(" s-1 ");
    REQUIRE(parcel.sealId() == "s-1");
    REQUIRE(parcel.revision() == 2);
    REQUIRE_NOTHROW(parcel.validateForPosting(500, Dimensions(50, 100, 200), 2));
    REQUIRE_THROWS_AS(parcel.addItem(item()), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(parcel.seal("again"), InvalidShipmentStateException);
}
TEST_CASE("Parcel rejects excessive inventory without partial additions", "[parcel]") {
    auto parcel = openParcel();
    parcel.addItem(item(2));
    const auto inventory = parcel.contents();
    REQUIRE_THROWS_AS(parcel.addItem(item(4)), ShipmentLimitExceededException);
    REQUIRE(parcel.contents() == inventory);
    REQUIRE(parcel.revision() == 1);
    REQUIRE_THROWS_AS(parcel.addItem(item(std::numeric_limits<std::int64_t>::max(), 2)),
                      std::overflow_error);
    REQUIRE(parcel.contents() == inventory);
}
TEST_CASE("Parcel validates sealed structure, quantity and revised gross weight", "[parcel]") {
    auto parcel = openParcel();
    REQUIRE_THROWS_AS(parcel.validateForPosting(500, Dimensions(200, 100, 50), 10),
                      InvalidShipmentStateException);
    parcel.addItem(item(2));
    REQUIRE_THROWS_AS(parcel.validateForPosting(500, Dimensions(200, 100, 50), 10),
                      InvalidShipmentStateException);
    parcel.seal("seal");
    REQUIRE_THROWS_AS(parcel.validateForPosting(500, Dimensions(200, 100, 50), 1),
                      ShipmentLimitExceededException);
    parcel.revise(100, DeliveryOptions{});
    REQUIRE_THROWS_AS(parcel.validateForPosting(500, Dimensions(200, 100, 50), 10),
                      ShipmentLimitExceededException);
    ShipmentRegistry registry("P");
    REQUIRE_THROWS_AS(registry.registerShipment(parcel, 500, Dimensions(200, 100, 50), 10),
                      ShipmentLimitExceededException);
    REQUIRE(registry.activeCount() == 0);
}
TEST_CASE("Parcel requires fragile handling and rigid packaging for fragile goods", "[parcel]") {
    const ContentItem glass("Стекло", 1, 10, Money(100), ContentCategory::FragileGoods);
    auto ordinary = openParcel();
    REQUIRE_THROWS_AS(ordinary.addItem(glass), InvalidShipmentDataException);
    auto padded = openParcel(DeliveryOptions(false, Money{}, false, false, true),
                             PackagingKind::PaddedEnvelope);
    REQUIRE_THROWS_AS(padded.addItem(glass), InvalidShipmentDataException);
    auto protectedParcel = openParcel(DeliveryOptions(false, Money{}, false, false, true));
    protectedParcel.addItem(glass);
    protectedParcel.seal("seal");
    REQUIRE_NOTHROW(protectedParcel.validateForPosting(500, Dimensions(200, 100, 50), 1));
    protectedParcel.revise(500, DeliveryOptions{});
    REQUIRE_THROWS_AS(protectedParcel.validateForPosting(500, Dimensions(200, 100, 50), 1),
                      InvalidShipmentDataException);
}
TEST_CASE("Parcel cloning preserves product type and independent contents", "[parcel]") {
    auto original = openParcel();
    original.addItem(item());
    auto cloned = original.clone();
    auto* copy = dynamic_cast<Parcel*>(cloned.get());
    REQUIRE(copy != nullptr);
    copy->addItem(item());
    REQUIRE(original.contents().size() == 1);
    REQUIRE(copy->contents().size() == 2);
    const auto person = test::customer();
    REQUIRE_THROWS_AS(Parcel(person, person, 1, DeliveryOptions{}, Dimensions(1, 1, 1),
                             static_cast<PackagingKind>(99)),
                      InvalidShipmentDataException);
}
