#include "../ConsoleApplication.hpp"
#include "shop/application/ReturnProcessing.hpp"

namespace shop::ui {
namespace {
ReturnReason reason(std::string_view text) {
    if (text == "changed-mind") {
        return ReturnReason::ChangedMind;
    }
    if (text == "defective") {
        return ReturnReason::Defective;
    }
    if (text == "wrong-item") {
        return ReturnReason::WrongItem;
    }
    throw InputError("Причина: changed-mind/defective/wrong-item");
}
ItemCondition condition(std::string_view text) {
    if (text == "unopened") {
        return ItemCondition::Unopened;
    }
    if (text == "opened") {
        return ItemCondition::Opened;
    }
    if (text == "defective") {
        return ItemCondition::Defective;
    }
    if (text == "incomplete") {
        return ItemCondition::Incomplete;
    }
    throw InputError("Состояние: unopened/opened/defective/incomplete");
}
} // namespace
void ConsoleApplication::returnCommands(Arguments& args) {
    const auto action = args.take("действие");
    ReturnProcessing process(state_);
    if (action == "list") {
        args.finish();
        for (const auto& [id, request] : state_.returnRequests()) {
            if (customerId_.empty() || request.customerId() == actor()) {
                output_ << id << " | заказ " << request.orderId() << " | статус "
                        << static_cast<int>(request.status()) << " | до " << request.deadline()
                        << '\n';
            }
        }
        return;
    }
    const auto id = args.take("заказ/заявка");
    if (action == "show") {
        args.finish();
        const auto found = state_.returnRequests().find(id);
        if (found == state_.returnRequests().end()) {
            throw InputError("Неизвестная заявка");
        }
        const auto& request = found->second;
        if (!customerId_.empty()) {
            request.assertOwner(actor());
        }
        output_ << id << " | до " << request.deadline() << " | максимум "
                << request.maximumRefund().format() << " | решение " << request.decisionReason()
                << '\n';
        for (const auto& line : request.lines()) {
            output_ << line.sku() << " x " << line.requestedQuantity() << " | смещение "
                    << line.alreadyReturnedOffset() << '\n';
        }
        return;
    }
    const auto operation = args.take("ID операции");
    if (action == "open" || action == "amend") {
        const auto customer = actor();
        std::vector<ReturnSelection> selections;
        while (!args.empty()) {
            const auto sku = args.take("SKU");
            const auto quantity = args.number("количество");
            const auto why = reason(args.take("причина"));
            selections.push_back({sku, quantity, why});
        }
        if (action == "open") {
            output_ << "Заявка: " << process.open(customer, id, selections, operation) << '\n';
        } else {
            process.amendSubmitted(customer, id, selections, operation);
            output_ << "Заявка изменена\n";
        }
    } else if (action == "withdraw") {
        args.finish();
        process.withdraw(actor(), id, operation);
        output_ << "Заявка отозвана\n";
    } else {
        requireOperator();
        if (action == "inspect") {
            std::vector<InspectionResult> results;
            while (!args.empty()) {
                const auto sku = args.take("SKU");
                const auto received = args.number("получено");
                const auto accepted = args.number("принято");
                const auto restock = args.number("пригодно");
                const auto itemState = condition(args.take("состояние"));
                results.emplace_back(sku, received, accepted, restock, itemState);
            }
            output_ << "Осмотр: " << process.inspect(id, results, operation) << '\n';
        } else if (action == "reject") {
            const auto why = args.take("причина отказа");
            args.finish();
            process.reject(id, why, operation);
            output_ << "Отказ оформлен\n";
        } else if (action == "complete") {
            args.finish();
            output_ << "Возмещено: " << process.complete(id, operation).format() << '\n';
        } else {
            throw InputError("Неизвестная команда возврата");
        }
    }
}
} // namespace shop::ui
