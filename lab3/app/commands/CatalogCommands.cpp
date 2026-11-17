#include "../ConsoleApplication.hpp"
#include "InputError.hpp"
#include <optional>

namespace shop::ui {
namespace {
std::map<std::string, std::string> attributes(const std::vector<std::string>& tokens) {
    std::map<std::string, std::string> result;
    for (const auto& token : tokens) {
        const auto equals = token.find('=');
        if (equals == std::string::npos || equals == 0 || equals + 1 == token.size() ||
            !result.emplace(token.substr(0, equals), token.substr(equals + 1)).second) {
            throw InputError("Ожидались уникальные name=value");
        }
    }
    return result;
}
std::optional<std::string> parentId(std::string id) {
    return id == "-" ? std::nullopt : std::optional(std::move(id));
}
} // namespace
void ConsoleApplication::catalogCommands(std::string_view command, Arguments& args) {
    const auto action = args.take("действие");
    if (command == "catalog") {
        if (action == "search") {
            const auto query = args.empty() ? std::string{} : args.take("запрос");
            const auto category = args.empty() ? std::nullopt : parentId(args.take("категория/-"));
            const auto filters = attributes(args.rest());
            for (const auto& item : state_.catalog().search(query, category, filters)) {
                output_ << item.id() << " | " << item.title() << '\n';
            }
        } else if (action == "variants") {
            const auto id = args.take("продукт");
            const auto filters = attributes(args.rest());
            for (const auto& item : state_.catalog().findCompatibleVariants(id, filters)) {
                output_ << item.sku() << " | " << item.label() << " | доступно "
                        << state_.inventory().available(item.sku()) << '\n';
            }
        } else if (action == "compare") {
            const auto ids = args.rest();
            const auto comparison =
                state_.catalog().compareProducts(ids, state_.prices(), state_.inventory());
            for (const auto& [id, column] : comparison.products) {
                output_ << id << " | " << column.title << " | ";
                if (column.priceRange) {
                    output_ << column.priceRange->first.format() << " .. "
                            << column.priceRange->second.format();
                } else {
                    output_ << "нет установленной цены";
                }
                output_ << '\n';
                for (const auto& [sku, quantity] : column.availableBySku) {
                    output_ << "  " << sku << ": " << quantity << '\n';
                }
            }
            for (const auto& [name, row] : comparison.attributes) {
                output_ << name << (row.comparable ? ": сопоставимо\n" : ": несовместимые типы\n");
            }
        } else {
            throw InputError("Неизвестная команда каталога");
        }
        return;
    }
    if (command == "coupon" && action == "usage") {
        const auto code = args.take("код");
        const auto customer = customerId_.empty() ? args.take("покупатель") : actor();
        args.finish();
        const auto usage = state_.coupons().usageSummary(code, customer);
        output_ << "Удержано: " << usage.held << ", оплачено: " << usage.confirmed
                << ", доступно покупателю: " << usage.available << '\n';
        return;
    }
    requireOperator();
    if (command == "category") {
        auto catalog = state_.catalog();
        if (action == "add") {
            const auto id = args.take("ID");
            const auto name = args.take("название");
            const auto parent = parentId(args.take("родитель/-"));
            args.finish();
            catalog.addCategory(Category(id, name, parent));
        } else if (action == "move") {
            const auto id = args.take("ID");
            const auto parent = parentId(args.take("родитель/-"));
            args.finish();
            catalog.moveCategory(id, parent);
        } else if (action == "archive") {
            const auto id = args.take("ID");
            args.finish();
            catalog.archiveCategory(id);
        } else if (action == "merge") {
            const auto source = args.take("источник");
            const auto target = args.take("назначение");
            args.finish();
            catalog.mergeCategories(source, target);
        } else {
            throw InputError("Неизвестная команда категории");
        }
        state_.replaceCatalog(catalog);
    } else if (command == "product") {
        auto catalog = state_.catalog();
        const auto id = args.take("ID продукта");
        if (action == "add") {
            const auto category = args.take("категория");
            const auto title = args.take("название");
            const auto description = args.take("описание");
            args.finish();
            catalog.addProduct(Product(id, category, title, description));
        } else if (action == "attribute") {
            const auto name = args.take("характеристика");
            const auto kind = args.take("text/integer");
            const auto required = args.boolean("обязательность yes/no");
            const auto allowed = args.rest();
            if (kind != "text" && kind != "integer") {
                throw InputError("Тип характеристики: text или integer");
            }
            auto schema = catalog.product(id).attributes();
            schema.emplace_back(name, kind == "text" ? AttributeKind::Text : AttributeKind::Integer,
                                required, std::set<std::string>(allowed.begin(), allowed.end()));
            catalog.replaceAttributeSchema(id, std::move(schema));
        } else if (action == "publish") {
            args.finish();
            catalog.publish(id);
        } else if (action == "archive") {
            args.finish();
            catalog.archiveProduct(id);
        } else if (action == "restore") {
            args.finish();
            catalog.restoreProductDraft(id);
        } else if (action == "move") {
            const auto products = args.rest();
            catalog.bulkMoveProducts(products, id);
        } else {
            throw InputError("Неизвестная команда продукта");
        }
        state_.replaceCatalog(catalog);
    } else if (command == "variant") {
        auto catalog = state_.catalog();
        const auto id = args.take("продукт/SKU");
        if (action == "add") {
            const auto sku = args.take("SKU");
            const auto label = args.take("название");
            const auto weight = args.number("масса");
            const std::array<std::int64_t, 3> dimensions{
                args.number("длина"), args.number("ширина"), args.number("высота")};
            const auto values = attributes(args.rest());
            catalog.addVariant(id, ProductVariant(sku, label, values, weight, dimensions));
        } else if (action == "enable") {
            const auto enabled = args.boolean("yes/no");
            args.finish();
            catalog.setVariantAvailability(id, enabled);
        } else {
            throw InputError("Неизвестная команда варианта");
        }
        state_.replaceCatalog(catalog);
    } else if (command == "price") {
        const auto id = args.take("SKU/категория");
        const auto amount = args.number("копейки/bps");
        args.finish();
        if (action == "set") {
            state_.changePrice(id, Money(amount));
        } else if (action == "category") {
            state_.applyCategoryPriceChange(id, amount);
        } else {
            throw InputError("Неизвестная команда цены");
        }
    } else if (command == "promotion") {
        if (action != "add") {
            throw InputError("Ожидалось promotion add");
        }
        const auto id = args.take("ID");
        const auto name = args.take("название");
        const auto kind = args.take("percent/fixed");
        const auto amount = args.number("значение");
        const Money minimum(args.number("минимум"));
        const TimeWindow window{args.number("начало"), args.number("конец")};
        const auto automatic = args.boolean("автоматическая yes/no");
        const auto categories = args.rest();
        if (kind != "percent" && kind != "fixed") {
            throw InputError("Вид акции: percent или fixed");
        }
        state_.addPromotion(Promotion(
            id, name, kind == "percent" ? PromotionKind::Percentage : PromotionKind::FixedAmount,
            amount, minimum, window, std::set<std::string>(categories.begin(), categories.end()),
            automatic));
    } else if (command == "coupon") {
        const auto code = args.take("код");
        if (action == "disable") {
            args.finish();
            state_.disableCoupon(code);
        } else if (action == "add") {
            const auto promotion = args.take("акция");
            const auto total = args.number("общий лимит");
            const auto personal = args.number("личный лимит");
            const TimeWindow window{args.number("начало"), args.number("конец")};
            args.finish();
            state_.registerCoupon(Coupon(code, promotion, total, personal, window));
        } else {
            throw InputError("Неизвестная команда купона");
        }
    } else if (command == "receipt") {
        if (action != "post") {
            throw InputError("Ожидалось receipt post");
        }
        const auto id = args.take("ID");
        const auto supplier = args.take("поставщик");
        StockReceipt receipt(id, supplier, state_.inventory().warehouse().id());
        while (!args.empty()) {
            const auto sku = args.take("SKU");
            const auto quantity = args.number("количество");
            const Money cost(args.number("закупочная цена"));
            const auto batch = args.take("партия");
            receipt.addLine(ReceiptLine(sku, quantity, cost, batch));
        }
        state_.postReceipt(receipt);
    } else if (command == "stock") {
        if (action == "show") {
            args.finish();
            for (const auto& [sku, item] : state_.inventory().items()) {
                output_ << sku << " | всего " << item.onHand() << " | резерв " << item.reserved()
                        << " | доступно " << item.available() << '\n';
            }
        } else if (action == "count") {
            const auto sku = args.take("SKU");
            const auto actual = args.number("факт");
            args.finish();
            state_.applyStockCount(sku, actual);
        } else if (action == "needs") {
            args.finish();
            for (const auto& [sku, quantity] : state_.inventory().replenishmentNeeds()) {
                output_ << sku << ": " << quantity << '\n';
            }
        } else if (action == "audit") {
            args.finish();
            const auto issues = state_.inventory().auditReservations(state_.orders());
            output_ << "Расхождений: " << issues.size() << '\n';
            for (const auto& issue : issues) {
                output_ << issue << '\n';
            }
        } else {
            throw InputError("Неизвестная команда склада");
        }
    }
    output_ << "Готово\n";
}
} // namespace shop::ui
