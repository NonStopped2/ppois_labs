#include "ConsoleApplication.hpp"
#include "SnapshotStore.hpp"
#include "shop/application/Checkout.hpp"
#include "shop/application/ReturnProcessing.hpp"
#include "shop/common/ShopException.hpp"

namespace shop::ui {
ConsoleApplication::ConsoleApplication(StoreState state, std::ostream& output)
    : state_(std::move(state)), output_(output) {}
const StoreState& ConsoleApplication::state() const noexcept {
    return state_;
}
std::size_t ConsoleApplication::errorCount() const noexcept {
    return errorCount_;
}
void ConsoleApplication::requireOperator() const {
    if (!customerId_.empty()) {
        throw InputError("Нужен режим оператора: as operator");
    }
}
const std::string& ConsoleApplication::actor() const {
    if (customerId_.empty()) {
        throw InputError("Выберите покупателя: as customer ID");
    }
    return customerId_;
}
Address ConsoleApplication::selectedAddress(std::string_view label) const {
    const auto book = state_.addresses(actor());
    if (label != "-") {
        return book.address(label);
    }
    if (!book.defaultAddressId()) {
        throw InputError("У покупателя нет адреса по умолчанию");
    }
    return book.address(*book.defaultAddressId());
}
bool ConsoleApplication::execute(std::string_view line) {
    try {
        const auto tokens = InputReader::tokenize(line);
        if (tokens.empty()) {
            return true;
        }
        const auto& command = tokens.front();
        Arguments args(tokens);
        if (command == "quit") {
            args.finish();
            return false;
        }
        if (command == "help") {
            args.finish();
            output_ << help();
        } else if (command == "as") {
            const auto mode = args.take("operator/customer");
            if (mode == "operator") {
                args.finish();
                customerId_.clear();
            } else if (mode == "customer") {
                const auto id = args.take("ID покупателя");
                args.finish();
                state_.customer(id);
                customerId_ = id;
            } else {
                throw InputError("Ожидалось as operator или as customer ID");
            }
            output_ << "Режим: " << (customerId_.empty() ? "operator" : customerId_) << '\n';
        } else if (command == "save") {
            requireOperator();
            const auto path = args.take("файл");
            args.finish();
            SnapshotStore::save(path, state_);
            output_ << "Снимок сохранён\n";
        } else if (command == "load") {
            requireOperator();
            const auto path = args.take("файл");
            const auto confirm = args.take("replace");
            args.finish();
            if (confirm != "replace") {
                throw InputError("Для полной замены состояния укажите replace");
            }
            SnapshotStore::load(path, state_);
            quote_.reset();
            deliveryChange_.reset();
            customerId_.clear();
            output_ << "Магазин восстановлен\n";
        } else if (command == "clock") {
            requireOperator();
            const auto action = args.take("show/advance/cleanup");
            if (action == "show") {
                args.finish();
            } else if (action == "advance") {
                const auto seconds = args.number("секунды");
                args.finish();
                state_.advanceClock(seconds);
                Checkout(state_).expireUnpaidOrders();
                ReturnProcessing(state_).closeExpiredUnreceived();
            } else if (action == "cleanup") {
                args.finish();
                Checkout(state_).expireUnpaidOrders();
                ReturnProcessing(state_).closeExpiredUnreceived();
            } else {
                throw InputError("Неизвестная команда часов");
            }
            output_ << "Время: " << state_.now() << '\n';
        } else if (command == "category" || command == "product" || command == "variant" ||
                   command == "catalog" || command == "price" || command == "promotion" ||
                   command == "coupon" || command == "stock" || command == "receipt") {
            catalogCommands(command, args);
        } else if (command == "customer" || command == "address") {
            customerCommands(command, args);
        } else if (command == "cart" || command == "wish") {
            cartCommands(command, args);
        } else if (command == "order" || command == "fulfill") {
            orderCommands(command, args);
        } else if (command == "return") {
            returnCommands(args);
        } else if (command == "review") {
            reviewCommands(args);
        } else if (command == "report") {
            reportCommand(args);
        } else {
            throw InputError("Неизвестная команда; help выводит справку");
        }
    } catch (const ShopException& error) {
        ++errorCount_;
        output_ << "Ошибка: " << error.what() << '\n';
    } catch (const InputError& error) {
        ++errorCount_;
        output_ << "Ошибка ввода: " << error.what() << '\n';
    }
    return true;
}
void ConsoleApplication::run(std::istream& input, bool prompts) {
    if (prompts) {
        output_ << "Интернет-магазин. help — справка, quit — выход.\n";
    }
    std::string line;
    while (true) {
        if (prompts) {
            output_ << "> " << std::flush;
        }
        if (!std::getline(input, line)) {
            break;
        }
        if (!execute(line)) {
            break;
        }
    }
    if (input.bad()) {
        throw InputError("Ошибка чтения команд");
    }
}
} // namespace shop::ui
