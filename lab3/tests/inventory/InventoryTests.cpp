#include "../Fixtures.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InsufficientStockException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/ReservationStateException.hpp"
#include "shop/inventory/Inventory.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace shop;
namespace {
StockReceipt receipt(std::int64_t quantity = 5) {
    StockReceipt value("receipt", "supplier", "warehouse");
    value.addLine(ReceiptLine("shirt-blue", quantity, Money(20), "batch"));
    return value;
}
Inventory inventory() {
    Inventory value(Warehouse("warehouse", "Склад", test::address()));
    value.postReceipt(receipt(), test::catalog());
    return value;
}
} // namespace

TEST_CASE("Stock item preserves physical and reserved quantities across failures") {
    StockItem item("sku", 10, 2, 5);
    REQUIRE(item.sku() == "sku");
    REQUIRE(item.onHand() == 10);
    REQUIRE(item.reserved() == 2);
    REQUIRE(item.reorderPoint() == 5);
    REQUIRE(item.available() == 8);
    item.receive(2);
    item.hold(3);
    item.release(1);
    item.consumeHeld(2);
    REQUIRE(item.onHand() == 10);
    REQUIRE(item.reserved() == 2);
    const auto before = item;
    REQUIRE_THROWS_AS(item.hold(9), InsufficientStockException);
    REQUIRE_THROWS_AS(item.release(3), ReservationStateException);
    REQUIRE_THROWS_AS(item.consumeHeld(3), ReservationStateException);
    REQUIRE_THROWS_AS(item.applyCount(1), InsufficientStockException);
    REQUIRE_THROWS_AS(item.receive(0), InvalidDomainDataException);
    REQUIRE(item == before);
    item.applyCount(2);
    REQUIRE(item.available() == 0);
    REQUIRE_THROWS_AS(StockItem("x", 0, 1), InvalidDomainDataException);
    StockItem maximal("x", std::numeric_limits<std::int64_t>::max());
    REQUIRE_THROWS_AS(maximal.receive(1), InvalidDomainDataException);
}

TEST_CASE("Reservation transitions distinguish expiry payment release and shipping") {
    StockReservation held("r", "o", "w", {{"sku", 2}}, 10, 20);
    REQUIRE(held.id() == "r");
    REQUIRE(held.orderId() == "o");
    REQUIRE(held.warehouseId() == "w");
    REQUIRE(held.quantities().at("sku") == 2);
    REQUIRE(held.createdAt() == 10);
    REQUIRE(held.expiresAt() == 20);
    REQUIRE(held.changedAt() == 10);
    REQUIRE(held.live());
    REQUIRE_THROWS_AS(held.confirm(9), ReservationStateException);
    REQUIRE_THROWS_AS(held.confirm(20), ReservationStateException);
    REQUIRE_THROWS_AS(held.consume(10), ReservationStateException);
    REQUIRE_THROWS_AS(held.expire(19), ReservationStateException);
    held.extend(11, 30);
    REQUIRE(held.expiresAt() == 30);
    REQUIRE_THROWS_AS(held.extend(12, 29), ReservationStateException);
    auto expired = held;
    expired.expire(30);
    REQUIRE(expired.status() == ReservationStatus::Expired);
    held.confirm(29);
    REQUIRE(held.status() == ReservationStatus::Confirmed);
    REQUIRE_THROWS_AS(held.expire(100), ReservationStateException);
    auto released = held;
    released.release(100);
    REQUIRE(released.status() == ReservationStatus::Released);
    held.consume(100);
    REQUIRE(held.status() == ReservationStatus::Consumed);
    REQUIRE_FALSE(held.live());
    REQUIRE_THROWS_AS(held.release(101), ReservationStateException);
    REQUIRE_THROWS_AS(StockReservation("r", "o", "w", {}, 0, 1), InvalidDomainDataException);
    REQUIRE_THROWS_AS(StockReservation("r", "o", "w", {{"s", 0}}, 0, 1),
                      InvalidDomainDataException);
}

TEST_CASE("Receipts consolidate compatible batches and posting compares replayed documents") {
    auto catalog = test::catalog();
    auto document = receipt();
    document.addLine(ReceiptLine("shirt-blue", 2, Money(20), "batch"));
    REQUIRE(document.validateForPosting() == Money(140));
    document.mergeDuplicateSku();
    REQUIRE(document.lines().size() == 1);
    REQUIRE(document.lines().front().quantity() == 7);
    REQUIRE(document.lines().front().sku() == "shirt-blue");
    REQUIRE(document.lines().front().purchaseUnitCost() == Money(20));
    REQUIRE(document.lines().front().batchReference() == "batch");
    auto conflicting = document;
    conflicting.addLine(ReceiptLine("shirt-blue", 1, Money(21), "batch"));
    const auto unchanged = conflicting;
    REQUIRE_THROWS_AS(conflicting.mergeDuplicateSku(), InvalidDomainDataException);
    REQUIRE(conflicting == unchanged);
    Inventory stock(Warehouse("warehouse", "Склад", test::address()));
    stock.postReceipt(document, catalog);
    stock.postReceipt(document, catalog);
    REQUIRE(stock.item("shirt-blue").onHand() == 7);
    REQUIRE(stock.postedReceipts().at("receipt").status() == ReceiptStatus::Posted);
    auto posted = stock.postedReceipts().at("receipt");
    REQUIRE_THROWS_AS(posted.addLine(document.lines().front()), InvalidDomainDataException);
    REQUIRE_THROWS_AS(posted.mergeDuplicateSku(), InvalidDomainDataException);
    REQUIRE_THROWS_AS(stock.postReceipt(receipt(8), catalog), DuplicateOperationException);
    StockReceipt invalid("bad", "supplier", "warehouse");
    invalid.addLine(ReceiptLine("shirt-blue", 1, Money(1), "b"));
    invalid.addLine(ReceiptLine("unknown", 1, Money(1), "b"));
    const auto before = stock;
    REQUIRE_THROWS_AS(stock.postReceipt(invalid, catalog), EntityNotFoundException);
    REQUIRE(stock == before);
    REQUIRE_THROWS_AS(StockReceipt("x", "s", "w").validateForPosting(), InvalidDomainDataException);
    REQUIRE_THROWS_AS(ReceiptLine("s", 0, Money(1), "b"), InvalidDomainDataException);
}

TEST_CASE("Inventory multi-SKU reservations roll back and return only unused held units") {
    auto catalog = test::catalog();
    catalog.addVariant("shirt",
                       ProductVariant("shirt-red", "red", {{"color", "red"}}, 1, {1, 1, 1}));
    auto stock = inventory();
    stock.addStockItem(catalog, "shirt-red", 2);
    REQUIRE(stock.warehouse().id() == "warehouse");
    REQUIRE(stock.warehouse().name() == "Склад");
    REQUIRE(stock.warehouse().address() == test::address());
    REQUIRE(stock.warehouse().acceptingReceipts());
    const auto before = stock;
    REQUIRE_THROWS_AS(stock.reserve("r", "o", {{"shirt-blue", 2}, {"shirt-red", 1}}, 10, 20),
                      InsufficientStockException);
    REQUIRE(stock == before);
    stock.reserve("r", "o", {{"shirt-blue", 2}}, 10, 20);
    REQUIRE(stock.item("shirt-blue").reserved() == 2);
    REQUIRE(stock.reservations().size() == 1);
    REQUIRE(stock.auditReservations().empty());
    REQUIRE_THROWS_AS(stock.reserve("r2", "o", {{"shirt-blue", 1}}, 10, 20),
                      DuplicateEntityException);
    REQUIRE_THROWS_AS(stock.applyStockCount("shirt-blue", 1), InsufficientStockException);
    stock.extend("r", 11, 30);
    stock.confirm("r", 29);
    stock.consume("r", 100);
    REQUIRE(stock.item("shirt-blue").onHand() == 3);
    REQUIRE(stock.reservation("r").status() == ReservationStatus::Consumed);
    REQUIRE_THROWS_AS(stock.release("r", 101), ReservationStateException);
    stock.reserve("r3", "o3", {{"shirt-blue", 1}}, 101, 110);
    stock.release("r3", 102);
    stock.reserve("r4", "o4", {{"shirt-blue", 1}}, 103, 110);
    stock.expire("r4", 110);
    stock.restock({{"shirt-blue", 1}});
    stock.applyStockCount("shirt-blue", 5);
    REQUIRE(stock.available("shirt-blue") == 5);
    REQUIRE(stock.available("unknown") == 0);
    REQUIRE(stock.items().size() == 2);
    REQUIRE(stock.replenishmentNeeds().at("shirt-red") == 2);
    REQUIRE(stock.auditReservations().empty());
    REQUIRE_THROWS_AS(stock.addStockItem(catalog, "shirt-red", 1), DuplicateEntityException);
    auto warehouse = stock.warehouse();
    warehouse.setAcceptingReceipts(false);
    REQUIRE_THROWS_AS(warehouse.assertMayReceive(), InvalidDomainDataException);
    Inventory closed(warehouse);
    REQUIRE_THROWS_AS(closed.postReceipt(receipt(), catalog), InvalidDomainDataException);
    auto wrong = receipt();
    Inventory other(Warehouse("other", "Склад", test::address()));
    REQUIRE_THROWS_AS(other.postReceipt(wrong, catalog), InvalidDomainDataException);
}
