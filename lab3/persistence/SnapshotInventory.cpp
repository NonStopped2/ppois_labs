#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"

namespace shop {
using namespace snapshot;
Inventory SnapshotCodec::readInventory(const Json& data) {
    object(data);
    const auto& savedWarehouse = object(data.at("warehouse"));
    Inventory result(Warehouse(string(savedWarehouse.at("id")), string(savedWarehouse.at("name")),
                               address(savedWarehouse.at("address")),
                               boolean(savedWarehouse.at("accepting_receipts"))));
    for (const auto& saved : array(data.at("items"))) {
        const StockItem item(string(saved.at("sku")), integer(saved.at("on_hand")),
                             integer(saved.at("reserved")), integer(saved.at("reorder_point")));
        insertUnique(result.items_, item.sku(), item);
    }
    for (const auto& saved : array(data.at("reservations"))) {
        StockReservation item(string(saved.at("id")), string(saved.at("order_id")),
                              string(saved.at("warehouse_id")), quantities(saved.at("quantities")),
                              integer(saved.at("created_at")), integer(saved.at("expires_at")));
        const auto changedAt = integer(saved.at("changed_at"));
        const auto status = enumeration(saved.at("status"), ReservationStatus::Expired);
        require(changedAt >= item.createdAt(), "Неверное время изменения резерва");
        switch (status) {
        case ReservationStatus::Held:
            require(changedAt < item.expiresAt(), "Неоплаченный резерв изменён после своего срока");
            item.changedAt_ = changedAt;
            break;
        case ReservationStatus::Confirmed: item.confirm(changedAt); break;
        case ReservationStatus::Consumed:
            item.confirm(item.createdAt());
            item.consume(changedAt);
            break;
        case ReservationStatus::Released: item.release(changedAt); break;
        case ReservationStatus::Expired: item.expire(changedAt); break;
        }
        insertUnique(result.reservations_, item.id(), item);
    }
    for (const auto& saved : array(data.at("receipts"))) {
        StockReceipt item(string(saved.at("id")), string(saved.at("supplier_reference")),
                          string(saved.at("warehouse_id")));
        for (const auto& line : array(saved.at("lines"))) {
            item.addLine(ReceiptLine(string(line.at("sku")), integer(line.at("quantity")),
                                     Money(integer(line.at("purchase_unit_cost"))),
                                     string(line.at("batch_reference"))));
        }
        item.validateForPosting();
        item.status_ = ReceiptStatus::Posted;
        require(enumeration(saved.at("status"), ReceiptStatus::Posted) == ReceiptStatus::Posted,
                "Непроведённый приход в реестре проведённых документов");
        insertUnique(result.postedReceipts_, item.id(), item);
    }
    return result;
}
Json SnapshotCodec::writeInventory(const Inventory& inventory) {
    const auto& warehouse = inventory.warehouse();
    return {{"warehouse",
             {{"id", warehouse.id()},
              {"name", warehouse.name()},
              {"address", value(warehouse.address())},
              {"accepting_receipts", warehouse.acceptingReceipts()}}},
            {"items", arrayOf(inventory.items(),
                              [](const auto& pair) {
                                  const auto& item = pair.second;
                                  return Json{{"sku", item.sku()},
                                              {"on_hand", item.onHand()},
                                              {"reserved", item.reserved()},
                                              {"reorder_point", item.reorderPoint()}};
                              })},
            {"reservations", arrayOf(inventory.reservations(),
                                     [](const auto& pair) {
                                         const auto& item = pair.second;
                                         return Json{{"id", item.id()},
                                                     {"order_id", item.orderId()},
                                                     {"warehouse_id", item.warehouseId()},
                                                     {"quantities", item.quantities()},
                                                     {"created_at", item.createdAt()},
                                                     {"expires_at", item.expiresAt()},
                                                     {"changed_at", item.changedAt()},
                                                     {"status", enumValue(item.status())}};
                                     })},
            {"receipts", arrayOf(inventory.postedReceipts(), [](const auto& pair) {
                 const auto& item = pair.second;
                 return Json{{"id", item.id()},
                             {"supplier_reference", item.supplierReference()},
                             {"warehouse_id", item.warehouseId()},
                             {"status", enumValue(item.status())},
                             {"lines", arrayOf(item.lines(), [](const auto& line) {
                                  return Json{
                                      {"sku", line.sku()},
                                      {"quantity", line.quantity()},
                                      {"purchase_unit_cost", line.purchaseUnitCost().kopecks()},
                                      {"batch_reference", line.batchReference()}};
                              })}};
             })}};
}
} // namespace shop
