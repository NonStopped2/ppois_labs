#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
using namespace snapshot;

ProductCatalog SnapshotCodec::readCatalog(const Json& data) {
    object(data);
    ProductCatalog result;
    const auto& categories = array(data.at("categories"));
    // При восстановлении сначала создаём родителей. Порядок записей в JSON произвольный.
    std::set<std::string> remaining;
    for (const auto& saved : categories) {
        const auto id = string(saved.at("id"));
        require(remaining.insert(id).second, "Повторная категория снимка");
    }
    while (!remaining.empty()) {
        bool progressed = false;
        for (const auto& saved : categories) {
            const auto id = string(saved.at("id"));
            if (!remaining.contains(id)) {
                continue;
            }
            const auto parent = optionalString(saved.at("parent_id"));
            if (parent && !result.categories().contains(*parent)) {
                continue;
            }
            insertUnique(result.categories_, id, Category(id, string(saved.at("name")), parent));
            remaining.erase(id);
            progressed = true;
        }
        require(progressed, "Цикл категорий или отсутствующий родитель");
    }
    for (const auto& saved : array(data.at("products"))) {
        auto schema = vectorOf<ProductAttribute>(saved.at("attributes"), attribute);
        Product item(string(saved.at("id")), string(saved.at("category_id")),
                     string(saved.at("title")), string(saved.at("description")), std::move(schema));
        for (const auto& option : array(saved.at("variants"))) {
            item.addVariant(variant(option));
        }
        const auto status = enumeration(saved.at("status"), ProductStatus::Archived);
        if (status == ProductStatus::Published) {
            item.publish();
        }
        if (status == ProductStatus::Archived) {
            item.archive();
        }
        result.addProduct(item);
    }
    // Архивируем потомков раньше родителей по тем же правилам, что и в каталоге.
    for (auto position = categories.size(); position > 0; --position) {
        bool changed = false;
        for (const auto& saved : categories) {
            const auto id = string(saved.at("id"));
            if (!boolean(saved.at("archived")) || result.category(id).archived()) {
                continue;
            }
            const bool activeChild = std::any_of(
                result.categories().begin(), result.categories().end(), [&id](const auto& pair) {
                    return pair.second.parentId() == id && !pair.second.archived();
                });
            if (!activeChild) {
                result.archiveCategory(id);
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }
    const auto revision = integer(data.at("revision"));
    for (const auto& [id, node] : result.categories_) {
        if (!node.archived()) {
            result.checkSiblingName(node, node.parentId());
        }
    }
    require(revision >=
                static_cast<std::int64_t>(result.categories().size() + result.products().size()),
            "Некорректная ревизия каталога");
    result.revision_ = revision;
    return result;
}
PriceList SnapshotCodec::readPrices(const Json& data) {
    object(data);
    PriceList result(Money(integer(data.at("minimum_unit_price"))));
    for (const auto& saved : array(data.at("entries"))) {
        const PriceEntry entry(string(saved.at("sku")), Money(integer(saved.at("unit_price"))),
                               integer(saved.at("changed_at")), integer(saved.at("revision")));
        require(entry.unitPrice() >= result.minimumUnitPrice(),
                "Цена меньше установленного минимума");
        insertUnique(result.entries_, entry.sku(), entry);
    }
    result.revision_ = integer(data.at("revision"));
    require(result.revision_ >= 0, "Отрицательная ревизия цен");
    for (const auto& [sku, entry] : result.entries_) {
        require(entry.revision() <= result.revision_, "Ревизия записи цены опережает реестр");
    }
    return result;
}
CouponBook SnapshotCodec::readCoupons(const Json& data) {
    object(data);
    CouponBook result;
    for (const auto& saved : array(data.at("codes"))) {
        const auto entry = coupon(saved);
        insertUnique(result.coupons_, entry.code(), entry);
    }
    const auto readUses = [](const auto& entries, auto& target, bool confirmed) {
        for (const auto& saved : array(entries)) {
            CouponUse use{string(saved.at("code")), string(saved.at("customer_id")),
                          integer(saved.at("held_at")), std::nullopt};
            if (!saved.at("confirmed_at").is_null()) {
                use.confirmedAt = integer(saved.at("confirmed_at"));
            }
            require(use.heldAt >= 0 && use.confirmedAt.has_value() == confirmed &&
                        (!use.confirmedAt || *use.confirmedAt >= use.heldAt),
                    "Некорректная хронология применения купона");
            insertUnique(target, string(saved.at("order_id")), use);
        }
    };
    readUses(data.at("held"), result.heldUses_, false);
    readUses(data.at("confirmed"), result.confirmedUses_, true);
    return result;
}
} // namespace shop
