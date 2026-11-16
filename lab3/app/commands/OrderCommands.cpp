#include "../ConsoleApplication.hpp"
#include "shop/application/Checkout.hpp"
#include "shop/application/OrderProcessing.hpp"
#include <algorithm>
#include <array>

namespace shop::ui {
namespace {
std::optional<OrderStatus> selectedStatus(std::string_view value) {
    if (value == "-") {
        return std::nullopt;
    }
    static constexpr std::array<std::string_view, 7> names{
        "unpaid", "paid", "picking", "shipped", "delivered", "cancelled", "expired"};
    const auto found = std::find(names.begin(), names.end(), value);
    if (found != names.end()) {
        return static_cast<OrderStatus>(found - names.begin());
    }
    throw InputError("Статус: unpaid/paid/picking/shipped/delivered/cancelled/expired/-");
}
} // namespace
void ConsoleApplication::orderCommands(std::string_view command, Arguments& args) {
    const auto action = args.take("действие");
    OrderProcessing process(state_);
    if (command == "fulfill") {
        requireOperator();
        const auto id = args.take("заказ");
        if (action == "show") {
            args.finish();
            const auto found = state_.fulfillments().find(id);
            if (found == state_.fulfillments().end()) {
                throw InputError("Исполнение ещё не создано");
            }
            const auto& shipment = found->second;
            output_ << shipment.id() << " | статус " << static_cast<int>(shipment.status())
                    << " | трек " << shipment.trackingNumber().value_or("-") << '\n';
            for (const auto& [sku, quantity] : shipment.pickedQuantities()) {
                output_ << sku << ": " << quantity << '/' << shipment.requiredQuantities().at(sku)
                        << '\n';
            }
            return;
        }
        if (action == "pick" || action == "undo") {
            const auto sku = args.take("SKU");
            const auto quantity = args.number("количество");
            const auto operation = args.take("ID операции");
            args.finish();
            if (action == "pick") {
                process.pick(id, sku, quantity, operation);
            } else {
                process.undoPick(id, sku, quantity, operation);
            }
        } else if (action == "ship") {
            const auto tracking = args.take("трек");
            const auto operation = args.take("ID операции");
            args.finish();
            process.dispatch(id, tracking, operation);
        } else {
            const auto operation = args.take("ID операции");
            args.finish();
            if (action == "start") {
                output_ << "Исполнение: " << process.startPicking(id, operation) << '\n';
            } else if (action == "pack") {
                process.pack(id, operation);
            } else if (action == "reopen") {
                process.reopenPicking(id, operation);
            } else {
                throw InputError("Неизвестная команда исполнения");
            }
        }
        output_ << "Готово\n";
        return;
    }
    if (action == "show") {
        const auto id = args.take("заказ");
        args.finish();
        const auto order = state_.orders().order(id);
        if (!customerId_.empty()) {
            order->assertOwner(actor());
        }
        showOrder(*order);
        return;
    }
    const auto customer = actor();
    if (action == "preview") {
        const auto option = args.take("доставка");
        const auto address = args.take("метка адреса/-");
        args.finish();
        auto result = Checkout(state_).preview(customer, selectedAddress(address), option);
        showQuote(result);
        quote_ = std::move(result);
    } else if (action == "place") {
        const auto operation = args.take("ID операции");
        args.finish();
        if (!quote_) {
            throw InputError("Сначала выполните order preview");
        }
        output_ << "Заказ: " << Checkout(state_).placeOrder(customer, *quote_, operation) << '\n';
    } else if (action == "list") {
        const auto filter = args.empty() ? std::nullopt : selectedStatus(args.take("статус/-"));
        std::optional<TimeWindow> period;
        if (!args.empty()) {
            period = TimeWindow{args.number("начало"), args.number("конец")};
        }
        args.finish();
        for (const auto& order : state_.orders().customerHistory(customer, filter, period)) {
            showOrder(*order);
        }
    } else if (action == "pay") {
        const auto id = args.take("заказ");
        const Money amount(args.number("копейки"));
        const auto method = args.take("card/transfer");
        const auto accepted = args.boolean("успех yes/no");
        const auto operation = args.take("ID операции");
        const auto reason = accepted ? std::string{} : args.take("причина отказа");
        args.finish();
        if (method != "card" && method != "transfer") {
            throw InputError("Способ: card или transfer");
        }
        const auto result = process.pay(customer, id, amount,
                                        method == "card" ? PaymentMethod::SimulatedCard
                                                         : PaymentMethod::SimulatedTransfer,
                                        accepted, reason, operation);
        output_ << (result.successful ? "Оплата успешна: " : "Оплата отклонена: ")
                << result.attemptId << '\n';
    } else if (action == "delivery-preview") {
        const auto id = args.take("заказ");
        const auto option = args.take("доставка");
        const auto address = args.take("метка адреса/-");
        args.finish();
        auto terms = process.previewUnpaidDelivery(customer, id, selectedAddress(address), option);
        output_ << "Новая доставка: " << terms.fee.format() << '\n';
        deliveryChange_ = std::make_pair(id, std::move(terms));
    } else if (action == "delivery-confirm") {
        const auto operation = args.take("ID операции");
        args.finish();
        if (!deliveryChange_) {
            throw InputError("Сначала выполните order delivery-preview");
        }
        process.changeUnpaidDelivery(customer, deliveryChange_->first, deliveryChange_->second,
                                     operation);
        output_ << "Доставка изменена\n";
    } else if (action == "repeat-plan") {
        const auto id = args.take("заказ");
        args.finish();
        for (const auto& entry : process.repeatOrderPlan(customer, id)) {
            output_ << entry.sku << " x " << entry.quantity
                    << (entry.available ? " доступно\n" : " недоступно\n");
        }
    } else if (action == "repeat") {
        const auto id = args.take("заказ");
        const auto operation = args.take("ID операции");
        const auto skus = args.rest();
        process.repeatOrder(customer, id, skus, operation);
        output_ << "Корзина дополнена\n";
    } else {
        const auto id = args.take("заказ");
        const auto operation = args.take("ID операции");
        args.finish();
        if (action == "cancel") {
            process.cancel(customer, id, operation);
        } else if (action == "extend") {
            process.extendPaymentDeadline(customer, id, operation);
        } else if (action == "deliver") {
            process.confirmDelivery(customer, id, operation);
        } else {
            throw InputError("Неизвестная команда заказа");
        }
        output_ << "Готово\n";
    }
}
} // namespace shop::ui
