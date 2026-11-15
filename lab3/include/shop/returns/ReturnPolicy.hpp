#pragma once
#include "shop/catalog/ProductCatalog.hpp"
#include "shop/common/TimeWindow.hpp"
#include <set>
#include <string>
#include <string_view>

namespace shop {
enum class ReturnReason { ChangedMind, Defective, WrongItem };
enum class ItemCondition { Unopened, Opened, Defective, Incomplete };
inline constexpr std::int64_t DefaultReturnWindowSeconds = 14 * 24 * 60 * 60;

/** This value is frozen into an offer and then into the purchase. */
class ReturnPolicy {
  public:
    explicit ReturnPolicy(std::int64_t returnWindow = DefaultReturnWindowSeconds,
                          std::set<std::string> nonReturnableCategories = {},
                          std::set<ReturnReason> allowedReasons = {ReturnReason::ChangedMind,
                                                                   ReturnReason::Defective,
                                                                   ReturnReason::WrongItem},
                          std::set<ItemCondition> restockableConditions = {ItemCondition::Unopened,
                                                                           ItemCondition::Opened});
    std::int64_t returnWindow() const noexcept;
    const std::set<std::string>& nonReturnableCategories() const noexcept;
    const std::set<ReturnReason>& allowedReasons() const noexcept;
    const std::set<ItemCondition>& restockableConditions() const noexcept;
    bool returnEligible(const ProductCatalog& catalog, std::string_view sku) const;
    Timestamp deadline(Timestamp deliveredAt) const;
    void assertEligible(bool purchasedEligibility, Timestamp deliveredAt, ReturnReason reason,
                        Timestamp now) const;
    bool classifyRestockability(ItemCondition condition) const;
    bool operator==(const ReturnPolicy&) const = default;

  private:
    std::int64_t returnWindow_;
    std::set<std::string> nonReturnableCategories_;
    std::set<ReturnReason> allowedReasons_;
    std::set<ItemCondition> restockableConditions_;
};
} // namespace shop
