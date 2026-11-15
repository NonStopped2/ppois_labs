#pragma once

#include "shop/application/StoreState.hpp"
#include <optional>
#include <type_traits>
#include <utility>

namespace shop::detail {
/** Lazily copies a touched root once. No callbacks or allocations occur during commit. */
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
class PreparedChange {
  public:
    explicit PreparedChange(StoreState& state)
        : orders_(state.orders_), inventory_(state.inventory_), coupons_(state.coupons_),
          payments_(state.payments_), carts_(state.carts_), wishlists_(state.wishlists_),
          loyalty_(state.loyaltyAccounts_), fulfillments_(state.fulfillments_),
          operations_(state.operations_) {}
    PreparedChange(const PreparedChange&) = delete;
    PreparedChange& operator=(const PreparedChange&) = delete;
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
    bool committed_ = false;
};
} // namespace shop
