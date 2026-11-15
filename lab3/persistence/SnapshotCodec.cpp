#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"

namespace shop {
using namespace snapshot;
Json SnapshotCodec::writeState(const StoreState& state) {
    return {{"format", "ppois-online-store"},
            {"version", 1},
            {"clock", state.now()},
            {"next_sequence", state.nextSequence_},
            {"configuration",
             {{"pricing", value(state.pricingPolicy_)},
              {"loyalty", value(state.loyaltyPolicy_)},
              {"delivery", value(state.deliveryPolicy_)},
              {"returns", value(state.returnPolicy_)},
              {"payment_seconds", state.paymentLifetime_},
              {"extension_seconds", state.paymentExtension_}}},
            {"catalog",
             {{"revision", state.catalog_.revision()},
              {"categories", arrayOf(state.catalog_.categories(),
                                     [](const auto& pair) {
                                         const auto& item = pair.second;
                                         return Json{{"id", item.id()},
                                                     {"name", item.name()},
                                                     {"parent_id", nullable(item.parentId())},
                                                     {"archived", item.archived()}};
                                     })},
              {"products", arrayOf(state.catalog_.products(),
                                   [](const auto& pair) {
                                       const auto& item = pair.second;
                                       return Json{{"id", item.id()},
                                                   {"category_id", item.categoryId()},
                                                   {"title", item.title()},
                                                   {"description", item.description()},
                                                   {"status", enumValue(item.status())},
                                                   {"attributes", arrayOf(item.attributes(),
                                                                          [](const auto& entry) {
                                                                              return value(entry);
                                                                          })},
                                                   {"variants",
                                                    arrayOf(item.variants(), [](const auto& entry) {
                                                        return value(entry);
                                                    })}};
                                   })}}},
            {"customers", writeCustomers(state)},
            {"prices",
             {{"revision", state.prices_.revision()},
              {"minimum_unit_price", state.prices_.minimumUnitPrice().kopecks()},
              {"entries", arrayOf(state.prices_.entries(),
                                  [](const auto& pair) {
                                      const auto& item = pair.second;
                                      return Json{{"sku", item.sku()},
                                                  {"unit_price", item.unitPrice().kopecks()},
                                                  {"changed_at", item.changedAt()},
                                                  {"revision", item.revision()}};
                                  })}}},
            {"promotions",
             arrayOf(state.promotions_, [](const auto& pair) { return value(pair.second); })},
            {"coupons",
             {{"codes", arrayOf(state.coupons_.coupons(),
                                [](const auto& pair) { return value(pair.second); })},
              {"held", arrayOf(state.coupons_.heldUses(),
                               [](const auto& pair) {
                                   const auto& use = pair.second;
                                   return Json{{"order_id", pair.first},
                                               {"code", use.code},
                                               {"customer_id", use.customerId},
                                               {"held_at", use.heldAt},
                                               {"confirmed_at", nullptr}};
                               })},
              {"confirmed", arrayOf(state.coupons_.confirmedUses(),
                                    [](const auto& pair) {
                                        const auto& use = pair.second;
                                        return Json{{"order_id", pair.first},
                                                    {"code", use.code},
                                                    {"customer_id", use.customerId},
                                                    {"held_at", use.heldAt},
                                                    {"confirmed_at", *use.confirmedAt}};
                                    })}}},
            {"inventory", writeInventory(state.inventory_)},
            {"orders",
             {{"revision", state.orders_.revision()},
              {"entries", arrayOf(state.orders_.orders(),
                                  [](const auto& pair) { return orderValue(*pair.second); })}}},
            {"payments", writePayments(state.payments_)},
            {"fulfillments",
             arrayOf(state.fulfillments_,
                     [](const auto& pair) {
                         const auto& item = pair.second;
                         return Json{{"id", item.id()},
                                     {"order_id", item.orderId()},
                                     {"warehouse_id", item.warehouseId()},
                                     {"required", item.requiredQuantities()},
                                     {"picked", item.pickedQuantities()},
                                     {"status", enumValue(item.status())},
                                     {"tracking_number", nullable(item.trackingNumber())},
                                     {"delivered_at", item.deliveredAt() ? Json(*item.deliveredAt())
                                                                         : Json(nullptr)}};
                     })},
            {"returns", writeReturns(state)},
            {"reviews",
             {{"revision", state.reviews_.revision()},
              {"entries", arrayOf(state.reviews_.reviews(),
                                  [](const auto& pair) {
                                      const auto& item = pair.second;
                                      return Json{{"id", item.id()},
                                                  {"customer_id", item.customerId()},
                                                  {"product_id", item.productId()},
                                                  {"order_id", item.orderId()},
                                                  {"rating", item.rating()},
                                                  {"text", item.text()},
                                                  {"moderation",
                                                   {{"visibility",
                                                     enumValue(item.moderationStatus().visibility)},
                                                    {"reason", item.moderationStatus().reason}}},
                                                  {"helpful_voters", item.helpfulVoters()}};
                                  })}}},
            {"operations", arrayOf(state.operations_, [](const auto& pair) {
                 return Json{{"id", pair.first},
                             {"signature", signatureValue(pair.second.signature)},
                             {"result", resultValue(pair.second.result)}};
             })}};
}
StoreState SnapshotCodec::readState(const Json& data) {
    object(data);
    require(string(data.at("format")) == "ppois-online-store" && integer(data.at("version")) == 1,
            "Неподдерживаемый формат или версия снимка");
    const auto& config = object(data.at("configuration"));
    StoreState state(readInventory(data.at("inventory")), deliveryPolicy(config.at("delivery")),
                     integer(data.at("clock")), pricingPolicy(config.at("pricing")),
                     loyaltyPolicy(config.at("loyalty")), returnsPolicy(config.at("returns")),
                     integer(config.at("payment_seconds")),
                     integer(config.at("extension_seconds")));
    state.catalog_ = readCatalog(data.at("catalog"));
    readCustomers(state, data.at("customers"));
    state.prices_ = readPrices(data.at("prices"));
    for (const auto& entry : array(data.at("promotions"))) {
        const auto item = promotion(entry);
        insertUnique(state.promotions_, item.id(), item);
    }
    state.coupons_ = readCoupons(data.at("coupons"));
    state.orders_ = readOrders(data.at("orders"));
    state.payments_ = readPayments(data.at("payments"));
    readFulfillments(state, data.at("fulfillments"));
    readReturns(state, data.at("returns"));
    state.reviews_ = readReviews(data.at("reviews"), state);
    for (const auto& entry : array(data.at("operations"))) {
        insertUnique(
            state.operations_, string(entry.at("id")),
            OperationRecord{signature(entry.at("signature")), operationResult(entry.at("result"))});
    }
    state.nextSequence_ = integer(data.at("next_sequence"));
    validate(state);
    // This also rejects unknown fields, changed caches and silently normalized identifiers.
    require(writeState(state) == data, "Снимок содержит неизвестные или противоречивые поля");
    return state;
}
std::string SnapshotCodec::encode(const StoreState& state) {
    try {
        validate(state);
        auto result = writeState(state).dump(2) + "\n";
        require(result.size() <= MaximumSnapshotBytes, "Снимок превышает допустимый размер");
        return result;
    } catch (const SnapshotException&) {
        throw;
    } catch (const ShopException& error) {
        throw SnapshotException(std::string("Некорректное состояние снимка: ") + error.what());
    } catch (const Json::exception& error) {
        throw SnapshotException(std::string("Ошибка кодирования JSON: ") + error.what());
    }
}
StoreState SnapshotCodec::decode(std::string_view document) {
    try {
        require(!document.empty() && document.size() <= MaximumSnapshotBytes,
                "Недопустимый размер JSON-снимка");
        std::vector<std::set<std::string>> objects;
        const auto callback = [&objects](int depth, Json::parse_event_t event, Json& parsed) {
            require(depth <= 128, "Слишком большая вложенность JSON");
            if (event == Json::parse_event_t::object_start) {
                objects.emplace_back();
            }
            if (event == Json::parse_event_t::key) {
                require(objects.back().insert(string(parsed)).second,
                        "Повторный ключ объекта JSON");
            }
            if (event == Json::parse_event_t::object_end) {
                objects.pop_back();
            }
            if (event == Json::parse_event_t::value) {
                require(!parsed.is_number_float(), "Дробные числа JSON не поддерживаются");
            }
            return true;
        };
        return readState(Json::parse(document.begin(), document.end(), callback));
    } catch (const SnapshotException&) {
        throw;
    } catch (const ShopException& error) {
        throw SnapshotException(std::string("Нарушен контракт снимка: ") + error.what());
    } catch (const Json::exception& error) {
        throw SnapshotException(std::string("Некорректный JSON-снимок: ") + error.what());
    } catch (const std::out_of_range& error) {
        throw SnapshotException(std::string("Неверная ссылка снимка: ") + error.what());
    }
}
} // namespace shop
