#include "ConsoleApplication.hpp"
#include <array>

namespace shop::ui {
void ConsoleApplication::showCart(const Cart& cart) {
    output_ << "Корзина " << cart.id() << ": " << cart.totalUnits() << " ед.\n";
    for (const auto& [sku, line] : cart.lines()) {
        output_ << sku << " x " << line.quantity() << '\n';
    }
    output_ << "Купон: " << cart.couponCode().value_or("-") << '\n';
}
void ConsoleApplication::showQuote(const PriceQuote& quote) {
    output_ << "Предложение для " << quote.customerId() << ", действительно до "
            << quote.validUntil() << '\n';
    for (const auto& line : quote.lines()) {
        output_ << line.sku() << " x " << line.quantity() << " | цена " << line.unitPrice().format()
                << " | скидка " << line.allocation().discountTotal().format() << " | чистая сумма "
                << line.netTotal().format() << '\n';
    }
    output_ << "Источник скидки: "
            << (quote.discountSource().id.empty() ? "нет" : quote.discountSource().id) << '\n';
    output_ << "Товары: " << quote.goodsNet().format()
            << ", доставка: " << quote.deliveryFee().format()
            << ", ИТОГО: " << quote.total().format() << '\n';
}
void ConsoleApplication::showOrder(const Order& order) {
    static constexpr std::array<std::string_view, 7> statuses{
        "AwaitingPayment", "Confirmed", "InFulfillment", "Shipped",
        "Delivered",       "Cancelled", "Expired"};
    output_ << order.id() << " | " << statuses.at(static_cast<std::size_t>(order.status())) << " | "
            << order.total().format() << " | оплатить до " << order.paymentDeadline() << '\n';
    output_ << "Адрес: " << order.deliveryAddress().city() << ", "
            << order.deliveryAddress().street() << ' ' << order.deliveryAddress().building()
            << " | доставка " << order.deliveryOptionId() << '\n';
    for (const auto& line : order.lines()) {
        output_ << "  " << line.sku() << " x " << line.quantity() << " | "
                << line.netTotal().format() << " | возврат "
                << (line.returnEligible() ? "разрешён" : "недоступен") << '\n';
    }
    for (const auto& event : order.history()) {
        output_ << "  t=" << event.timestamp() << " event=" << static_cast<int>(event.kind())
                << " op=" << event.operationId() << " ref=" << event.relatedEntityId() << '\n';
    }
}
} // namespace shop::ui
