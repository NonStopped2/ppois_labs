#include "../PurchaseFixtures.hpp"
#include "shop/common/FulfillmentStateException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/delivery/Fulfillment.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE("Fulfillment picks exactly the purchase and allows correction only before handover") {
    REQUIRE_THROWS_AS(Fulfillment("f", test::order(), "w"), FulfillmentStateException);
    Fulfillment shipment("f", test::paidOrder(), "w");
    REQUIRE(shipment.id() == "f");
    REQUIRE(shipment.orderId() == "order");
    REQUIRE(shipment.warehouseId() == "w");
    REQUIRE(shipment.requiredQuantities().at("shirt-blue") == 3);
    REQUIRE(shipment.pickedQuantities().at("shirt-blue") == 0);
    REQUIRE(shipment.status() == FulfillmentStatus::Created);
    REQUIRE_FALSE(shipment.trackingNumber());
    REQUIRE_FALSE(shipment.deliveredAt());
    REQUIRE_THROWS_AS(shipment.pick("shirt-blue", 1), FulfillmentStateException);
    shipment.startPicking();
    shipment.pick("shirt-blue", 2);
    const auto before = shipment;
    REQUIRE_THROWS_AS(shipment.pick("shirt-blue", 2), FulfillmentStateException);
    REQUIRE_THROWS_AS(shipment.undoPick("shirt-blue", 3), FulfillmentStateException);
    REQUIRE_THROWS_AS(shipment.pack(), FulfillmentStateException);
    REQUIRE(shipment == before);
    shipment.undoPick("shirt-blue", 1);
    shipment.pick("shirt-blue", 2);
    shipment.pack();
    REQUIRE(shipment.status() == FulfillmentStatus::Packed);
    shipment.reopenPicking();
    REQUIRE(shipment.pickedQuantities().at("shirt-blue") == 3);
    shipment.undoPick("shirt-blue", 1);
    REQUIRE_THROWS_AS(shipment.pack(), FulfillmentStateException);
    shipment.pick("shirt-blue", 1);
    shipment.pack();
    shipment.handover("TRACK-1");
    REQUIRE(shipment.trackingNumber() == "TRACK-1");
    REQUIRE(shipment.status() == FulfillmentStatus::HandedOver);
    REQUIRE_THROWS_AS(shipment.cancel(), FulfillmentStateException);
    REQUIRE_THROWS_AS(shipment.reopenPicking(), FulfillmentStateException);
    REQUIRE_THROWS_AS(shipment.deliver(-1), InvalidDomainDataException);
    shipment.deliver(20);
    REQUIRE(shipment.status() == FulfillmentStatus::Delivered);
    REQUIRE(shipment.deliveredAt() == 20);
    REQUIRE_THROWS_AS(shipment.deliver(21), FulfillmentStateException);
}

TEST_CASE("Fulfillment cancellation is terminal before shipping") {
    Fulfillment created("f", test::paidOrder(), "w");
    auto picking = created;
    auto packed = created;
    created.cancel();
    REQUIRE(created.status() == FulfillmentStatus::Cancelled);
    picking.startPicking();
    picking.cancel();
    REQUIRE(picking.status() == FulfillmentStatus::Cancelled);
    packed.startPicking();
    packed.pick("shirt-blue", 3);
    packed.pack();
    packed.cancel();
    REQUIRE(packed.status() == FulfillmentStatus::Cancelled);
    REQUIRE_THROWS_AS(packed.startPicking(), FulfillmentStateException);
    REQUIRE_THROWS_AS(packed.handover("TRACK"), FulfillmentStateException);
}
