#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"
#include "shop/common/Lookup.hpp"
#include <charconv>
#include <numeric>

namespace shop::snapshot {
Money requestAmount(const ReturnRequest& request, const ReturnInspection& inspection,
                    const Order& purchase) {
    return std::accumulate(
        request.lines().begin(), request.lines().end(), Money{},
        [&purchase, &inspection](Money amount, const auto& line) {
            return amount.checkedAdd(
                purchase.line(line.sku())
                    .allocation()
                    .netForUnitRange(
                        line.alreadyReturnedOffset(),
                        detail::get(inspection.results(), line.sku()).acceptedQuantity()));
        });
}
void checkCounter(std::string_view id, std::string_view prefix, std::int64_t next) {
    const auto expected = std::string(prefix) + "-";
    if (!id.starts_with(expected)) {
        return;
    }
    const auto number = id.substr(expected.size());
    std::int64_t sequence = 0;
    const auto parsed = std::from_chars(number.data(), number.data() + number.size(), sequence);
    require(parsed.ec == std::errc{} && parsed.ptr == number.data() + number.size() &&
                sequence > 0 && sequence < next,
            "Счётчик ID не опережает использованный идентификатор");
}
} // namespace shop::snapshot

namespace shop {
using namespace snapshot;
void SnapshotCodec::validate(const StoreState& state) {
    require(state.nextSequence_ > 0, "Неположительный счётчик ID");
    validateCustomers(state);
    validateOrders(state);
    validatePayments(state);
    validateReturns(state);
    validateOperations(state);
    for (const auto& [sku, entry] : state.prices_.entries()) {
        state.catalog_.variant(sku);
        require(entry.changedAt() <= state.now(), "Цена установлена в будущем");
    }
    for (const auto& [id, offer] : state.promotions_) {
        for (const auto& category : offer.categoryIds()) {
            state.catalog_.category(category);
        }
    }
    for (const auto& category : state.returnPolicy_.nonReturnableCategories()) {
        state.catalog_.category(category);
    }
    for (const auto& [sku, item] : state.inventory_.items()) {
        state.catalog_.variant(sku);
    }
    for (const auto& [id, receipt] : state.inventory_.postedReceipts()) {
        require(receipt.warehouseId() == state.inventory_.warehouse().id(),
                "Приход относится к другому складу");
        for (const auto& line : receipt.lines()) {
            state.catalog_.variant(line.sku());
        }
    }
    for (const auto& [id, review] : state.reviews_.reviews()) {
        state.customer(review.customerId());
        const auto order = state.orders_.order(review.orderId());
        const auto product = state.catalog_.product(review.productId());
        require(order->status() == OrderStatus::Delivered &&
                    order->customerId() == review.customerId(),
                "Отзыв не подтверждён полученной покупкой");
        const bool purchased =
            std::any_of(order->lines().begin(), order->lines().end(), [&product](const auto& line) {
                return std::any_of(
                    product.variants().begin(), product.variants().end(),
                    [&line](const auto& variant) { return variant.sku() == line.sku(); });
            });
        require(purchased, "В покупке отсутствует продукт отзыва");
        for (const auto& voter : review.helpfulVoters()) {
            state.customer(voter);
            require(voter != review.customerId(), "Автор проголосовал за себя");
        }
        checkCounter(id, "review", state.nextSequence_);
    }
}
void SnapshotCodec::validateCustomers(const StoreState& state) {
    const auto count = state.customers_.size();
    require(state.addressBooks_.size() == count && state.wishlists_.size() == count &&
                state.carts_.size() == count && state.loyaltyAccounts_.size() == count,
            "Не совпадают реестры покупателей");
    std::set<std::string> cartIds;
    for (const auto& [id, customer] : state.customers_) {
        require(customer.id() == id && customer.registeredAt() <= state.now(),
                "Некорректный покупатель снимка");
        const auto& cart = detail::get(state.carts_, id);
        const auto& addresses = detail::get(state.addressBooks_, id);
        const auto& wishlist = detail::get(state.wishlists_, id);
        const auto& account = detail::get(state.loyaltyAccounts_, id);
        require(cart.customerId() == id && addresses.customerId() == id &&
                    wishlist.customerId() == id && account.customerId() == id &&
                    cartIds.insert(cart.id()).second,
                "Чужой или повторный реестр покупателя");
        require(cart.totalUnits() <= state.pricingPolicy_.maximumCartUnits(),
                "Корзина превышает ограничение магазина");
        for (const auto& [sku, line] : cart.lines()) {
            state.catalog_.variant(sku);
            require(line.addedAt() <= state.now(), "Позиция корзины добавлена в будущем");
        }
        if (cart.couponCode()) {
            require(Coupon::normalizeCode(*cart.couponCode()) == *cart.couponCode(),
                    "Ненормализованный код корзины");
        }
        for (const auto& product : wishlist.productIds()) {
            state.catalog_.product(product);
        }
        std::map<std::string, Money> expectedPurchases;
        std::set<std::string> expectedOperations;
        for (const auto& [orderId, order] : state.orders_.orders()) {
            if (order->customerId() != id || order->status() != OrderStatus::Delivered) {
                continue;
            }
            Money returned;
            for (const auto& event : order->history()) {
                if (event.kind() == OrderEventKind::Delivered) {
                    const auto& operation =
                        detail::get(account.processedOperations_, event.operationId());
                    require(operation ==
                                LoyaltyAccount::Operation{orderId, order->goodsNet(), false},
                            "Операция получения не совпадает со счётом лояльности");
                    expectedOperations.insert(event.operationId());
                }
                if (event.kind() == OrderEventKind::ReturnCompleted) {
                    const auto& request =
                        detail::get(state.returnRequests_, event.relatedEntityId());
                    require(request.inspectionId().has_value(), "Нет осмотра учтённого возврата");
                    const auto amount = requestAmount(
                        request, detail::get(state.inspections_, *request.inspectionId()), *order);
                    const auto& operation =
                        detail::get(account.processedOperations_, event.operationId());
                    require(operation == LoyaltyAccount::Operation{orderId, amount, true},
                            "Операция возврата не совпадает со счётом лояльности");
                    expectedOperations.insert(event.operationId());
                    returned = returned.checkedAdd(amount);
                }
            }
            expectedPurchases.emplace(orderId, order->goodsNet().checkedSubtract(returned));
        }
        require(expectedPurchases == account.netDeliveredPurchases_ &&
                    expectedOperations.size() == account.processedOperations_.size(),
                "Оборот лояльности не совпадает с историей получений и возвратов");
        Money sum;
        for (const auto& [orderId, amount] : expectedPurchases) {
            require(amount >= Money{}, "Отрицательный чистый оборот");
            sum = sum.checkedAdd(amount);
        }
        require(sum == account.qualifyingSpend(), "Неверный кеш общего оборота");
    }
}
void SnapshotCodec::validateOrders(const StoreState& state) {
    require(state.inventory_.auditReservations(state.orders_).empty(),
            "Резервы склада не согласованы с заказами");
    std::set<std::string> tracking;
    std::set<std::string> expectedFulfillments;
    for (const auto& [id, order] : state.orders_.orders()) {
        const auto customer = state.customer(order->customerId());
        require(order->createdAt() >= customer.registeredAt() && order->createdAt() <= state.now(),
                "Некорректное время покупки");
        const auto reservation = state.inventory_.reservation(order->reservationId());
        require(reservation.createdAt() == order->createdAt() &&
                    reservation.changedAt() <= state.now(),
                "Некорректное время резерва");
        Timestamp reservationChanged = order->createdAt();
        bool paid = false;
        std::optional<std::string> fulfillmentId;
        for (const auto& event : order->history()) {
            require(event.timestamp() <= state.now(), "Событие заказа лежит в будущем");
            switch (event.kind()) {
            case OrderEventKind::PaymentConfirmed:
                paid = true;
                reservationChanged = event.timestamp();
                break;
            case OrderEventKind::DeadlineExtended:
            case OrderEventKind::Cancelled:
            case OrderEventKind::Expired:
            case OrderEventKind::Shipped: reservationChanged = event.timestamp(); break;
            case OrderEventKind::FulfillmentStarted: fulfillmentId = event.relatedEntityId(); break;
            default: break;
            }
        }
        require(reservation.changedAt() == reservationChanged,
                "Время изменения резерва не совпадает с историей");
        if (fulfillmentId) {
            const auto& shipment = detail::get(state.fulfillments_, id);
            expectedFulfillments.insert(id);
            require(shipment.id() == *fulfillmentId && shipment.orderId() == id &&
                        shipment.warehouseId() == state.inventory_.warehouse().id(),
                    "Исполнение относится к другому заказу или складу");
            const auto status = order->status();
            for (const auto& event : order->history()) {
                if (event.kind() == OrderEventKind::Shipped ||
                    event.kind() == OrderEventKind::Delivered) {
                    require(event.relatedEntityId() == shipment.id(),
                            "Событие доставки ссылается на другое исполнение");
                }
            }
            const auto shipped = shipment.status();
            require(
                (status == OrderStatus::InFulfillment &&
                 (shipped == FulfillmentStatus::Picking || shipped == FulfillmentStatus::Packed)) ||
                    (status == OrderStatus::Shipped && shipped == FulfillmentStatus::HandedOver) ||
                    (status == OrderStatus::Delivered && shipped == FulfillmentStatus::Delivered) ||
                    (status == OrderStatus::Cancelled && shipped == FulfillmentStatus::Cancelled),
                "Состояния заказа и исполнения не совпадают");
            require(shipment.deliveredAt() == order->deliveredAt(),
                    "Время получения не совпадает с исполнением");
            if (shipment.trackingNumber()) {
                require(tracking.insert(*shipment.trackingNumber()).second, "Повторный трек-номер");
            }
            checkCounter(shipment.id(), "fulfillment", state.nextSequence_);
        }
        const auto& source = order->discountSource();
        if (source.kind == DiscountKind::Promotion || source.kind == DiscountKind::Coupon) {
            detail::get(state.promotions_, source.id);
        }
        if (source.kind == DiscountKind::Loyalty) {
            require(source.id == "loyalty", "Неизвестный источник лояльности");
        }
        if (source.couponCode) {
            const auto coupon = state.coupons_.coupon(*source.couponCode);
            require(coupon.promotionId() == source.id, "Купон заказа относится к другой акции");
            const bool held = order->status() == OrderStatus::AwaitingPayment;
            const auto& uses = paid ? state.coupons_.confirmedUses() : state.coupons_.heldUses();
            if (held || paid) {
                const auto& use = detail::get(uses, id);
                require(use.code == *source.couponCode && use.customerId == order->customerId() &&
                            use.heldAt == order->createdAt(),
                        "Применение купона не совпадает с покупкой");
            } else {
                require(!state.coupons_.heldUses().contains(id) &&
                            !state.coupons_.confirmedUses().contains(id),
                        "У закрытого неоплаченного заказа остался купон");
            }
        }
        checkCounter(id, "order", state.nextSequence_);
        checkCounter(order->reservationId(), "reservation", state.nextSequence_);
    }
    require(expectedFulfillments.size() == state.fulfillments_.size(),
            "Лишнее исполнение без события заказа");
    for (const auto& [code, coupon] : state.coupons_.coupons()) {
        detail::get(state.promotions_, coupon.promotionId());
        for (const auto& [id, customer] : state.customers_) {
            require(state.coupons_.usageSummary(code, id).available >= 0, "Превышена квота купона");
        }
    }
    const auto checkUses = [&state](const auto& uses, bool confirmed) {
        for (const auto& [orderId, use] : uses) {
            const auto order = state.orders_.order(orderId);
            const auto coupon = state.coupons_.coupon(use.code);
            require(order->discountSource().couponCode == use.code &&
                        order->customerId() == use.customerId && use.heldAt == order->createdAt() &&
                        use.confirmedAt.has_value() == confirmed &&
                        coupon.window().contains(use.heldAt) &&
                        detail::get(state.promotions_, coupon.promotionId())
                            .window()
                            .contains(use.heldAt),
                    "Неверная ссылка применения купона");
            if (confirmed) {
                const auto payment = state.payments_.findPaymentForOrder(orderId);
                require(payment && payment->capturedAt() == *use.confirmedAt &&
                            *use.confirmedAt <= state.now(),
                        "Подтверждение купона не совпадает с оплатой");
            } else {
                require(order->status() == OrderStatus::AwaitingPayment &&
                            !state.coupons_.confirmedUses().contains(orderId),
                        "Удержание купона у завершённого заказа");
            }
        }
    };
    checkUses(state.coupons_.heldUses(), false);
    checkUses(state.coupons_.confirmedUses(), true);
}
} // namespace shop
