#pragma once

#include "shop/application/StoreState.hpp"
#include <optional>
#include <type_traits>
#include <utility>

namespace shop::detail {
/** Копирует затронутое значение по первому запросу. Фиксация обходится без аллокаций и вызовов
 * извне. */
template <class T> class StagedValue {
  public:
    explicit StagedValue(T& target) : target_(target) {}
    T& write() {
        if (!candidate_) {
            candidate_.emplace(target_);
        }
        return *candidate_;
    }
    void commit() noexcept {
        static_assert(std::is_nothrow_swappable_v<T>);
        if (candidate_) {
            using std::swap;
            swap(target_, *candidate_);
        }
    }

  private:
    T& target_;
    std::optional<T> candidate_;
};
} // namespace shop::detail

namespace shop {
/** PreparedChange.
 * Лениво копирует затронутые реестры. Фиксация обменом публикует документы и счётчик ID
 * одновременно.
 */
class PreparedChange {
  public:
    explicit PreparedChange(StoreState& state)
        : orders_(state.orders_), inventory_(state.inventory_), coupons_(state.coupons_),
          payments_(state.payments_), carts_(state.carts_), wishlists_(state.wishlists_),
          loyalty_(state.loyaltyAccounts_), fulfillments_(state.fulfillments_),
          operations_(state.operations_), returns_(state.returnRequests_),
          inspections_(state.inspections_), reviews_(state.reviews_),
          nextSequence_(state.nextSequence_) {}
    PreparedChange(const PreparedChange&) = delete;
    PreparedChange& operator=(const PreparedChange&) = delete;
    std::string nextId(std::string_view prefix) {
        return StoreState::allocateId(nextSequence_.write(), prefix);
    }
    OrderBook& orders() {
        return orders_.write();
    }
    Inventory& inventory() {
        return inventory_.write();
    }
    CouponBook& coupons() {
        return coupons_.write();
    }
    PaymentBook& payments() {
        return payments_.write();
    }
    std::map<std::string, Cart>& carts() {
        return carts_.write();
    }
    std::map<std::string, Wishlist>& wishlists() {
        return wishlists_.write();
    }
    std::map<std::string, LoyaltyAccount>& loyalty() {
        return loyalty_.write();
    }
    std::map<std::string, Fulfillment>& fulfillments() {
        return fulfillments_.write();
    }
    OperationJournal& operations() {
        return operations_.write();
    }
    std::map<std::string, ReturnRequest>& returns() {
        return returns_.write();
    }
    std::map<std::string, ReturnInspection>& inspections() {
        return inspections_.write();
    }
    ReviewBook& reviews() {
        return reviews_.write();
    }
    void commit() noexcept {
        if (committed_) {
            return;
        }
        orders_.commit();
        inventory_.commit();
        coupons_.commit();
        payments_.commit();
        carts_.commit();
        wishlists_.commit();
        loyalty_.commit();
        fulfillments_.commit();
        operations_.commit();
        returns_.commit();
        inspections_.commit();
        reviews_.commit();
        nextSequence_.commit();
        committed_ = true;
    }

  private:
    detail::StagedValue<OrderBook> orders_;
    detail::StagedValue<Inventory> inventory_;
    detail::StagedValue<CouponBook> coupons_;
    detail::StagedValue<PaymentBook> payments_;
    detail::StagedValue<std::map<std::string, Cart>> carts_;
    detail::StagedValue<std::map<std::string, Wishlist>> wishlists_;
    detail::StagedValue<std::map<std::string, LoyaltyAccount>> loyalty_;
    detail::StagedValue<std::map<std::string, Fulfillment>> fulfillments_;
    detail::StagedValue<OperationJournal> operations_;
    detail::StagedValue<std::map<std::string, ReturnRequest>> returns_;
    detail::StagedValue<std::map<std::string, ReturnInspection>> inspections_;
    detail::StagedValue<ReviewBook> reviews_;
    // Идентификаторы публикуются одновременно с документами, в которых они используются.
    detail::StagedValue<std::int64_t> nextSequence_;
    bool committed_ = false;
};
} // namespace shop
