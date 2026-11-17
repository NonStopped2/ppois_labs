#include "shop/pricing/PriceList.hpp"
#include "shop/common/CatalogRuleException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

PriceList::PriceList(Money minimumUnitPrice) : minimumUnitPrice_(minimumUnitPrice) {
    detail::require(minimumUnitPrice > Money{}, "Минимальная цена должна быть положительной");
}
void PriceList::changePrice(const ProductCatalog& catalog, std::string_view sku, Money amount,
                            Timestamp now) {
    catalog.variant(sku);
    detail::require(amount >= minimumUnitPrice_ && now >= 0, "Недопустимая цена или время");
    const auto found = entries_.find(std::string(sku));
    const auto entryRevision =
        found == entries_.end() ? 1 : detail::addNonnegative(found->second.revision(), 1);
    detail::require(found == entries_.end() || now >= found->second.changedAt(),
                    "Цена не изменяется задним числом");
    PriceEntry entry(sku, amount, now, entryRevision);
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    auto candidate = entries_;
    candidate.insert_or_assign(entry.sku(), entry);
    entries_.swap(candidate);
    revision_ = nextRevision;
}

// Сначала рассчитываем новые цены для всей категории, затем заменяем текущий список.
void PriceList::applyCategoryPriceChange(const ProductCatalog& catalog, std::string_view categoryId,
                                         std::int64_t changeBps, Timestamp now) {
    detail::require(changeBps >= -10000 && changeBps <= 10000 && now >= 0,
                    "Изменение цены должно быть в диапазоне -100%..100%");
    detail::check<CatalogRuleException>(!catalog.category(categoryId).archived(),
                                        "Категория архивирована");
    auto candidate = entries_;
    for (const auto& [sku, entry] : entries_) {
        const auto item = catalog.productForSku(sku);
        if (item.status() == ProductStatus::Archived ||
            !catalog.isInCategory(item.categoryId(), categoryId)) {
            continue;
        }
        detail::require(now >= entry.changedAt(), "Нельзя изменить цену задним числом");
        const auto delta =
            entry.unitPrice().percentageFloor(changeBps < 0 ? -changeBps : changeBps);
        const auto amount = changeBps < 0 ? entry.unitPrice().checkedSubtract(delta)
                                          : entry.unitPrice().checkedAdd(delta);
        detail::require(amount >= minimumUnitPrice_,
                        "Пакетное изменение нарушает минимальную цену");
        candidate.at(sku) =
            PriceEntry(sku, amount, now, detail::addNonnegative(entry.revision(), 1));
    }
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    entries_.swap(candidate);
    revision_ = nextRevision;
}

void PriceList::removePriceForArchivedVariant(const ProductCatalog& catalog, std::string_view sku) {
    const auto item = catalog.productForSku(sku);
    detail::check<CatalogRuleException>(item.status() != ProductStatus::Published ||
                                            !catalog.variant(sku).saleEnabled(),
                                        "Нельзя убрать цену продаваемого варианта");
    detail::get(entries_, sku);
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    entries_.erase(std::string(sku));
    revision_ = nextRevision;
}
PriceEntry PriceList::price(std::string_view sku) const {
    return detail::get(entries_, sku);
}
const std::map<std::string, PriceEntry>& PriceList::entries() const noexcept {
    return entries_;
}
std::int64_t PriceList::revision() const noexcept {
    return revision_;
}
Money PriceList::minimumUnitPrice() const noexcept {
    return minimumUnitPrice_;
}

} // namespace shop
