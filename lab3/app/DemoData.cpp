#include "DemoData.hpp"
#include <limits>

namespace shop::ui {
StoreState makeDemoStore() {
    const Address warehouseAddress("BY", "220001", "Минск", "Складская", "1");
    const DeliveryOption standard("standard", "Стандарт", {}, 30000, 1500, Money(500), Money(5000));
    const DeliveryOption express("express", "Экспресс", {"22"}, 10000, 1000, Money(800),
                                 Money(10000));
    const DeliveryOption pickup("pickup", "Самовывоз", {}, 100000, 3000, Money{}, Money{});
    StoreState state(
        Inventory(Warehouse("warehouse", "Основной склад", warehouseAddress)),
        DeliveryPolicy(
            {standard, express, pickup}, 3600,
            {{"standard", {86400, 172800}}, {"express", {3600, 7200}}, {"pickup", {0, 1}}}),
        1000);
    ProductCatalog catalog;
    catalog.addCategory(Category("root", "Каталог"));
    catalog.addCategory(Category("clothes", "Одежда", "root"));
    Product shirt("shirt", "clothes", "Футболка", "Хлопковая футболка",
                  {ProductAttribute("color", AttributeKind::Text, true, {"blue", "red"})});
    shirt.addVariant(
        ProductVariant("SHIRT-BLUE", "Синяя", {{"color", "blue"}}, 200, {200, 100, 20}));
    shirt.addVariant(
        ProductVariant("SHIRT-RED", "Красная", {{"color", "red"}}, 200, {200, 100, 20}));
    shirt.publish();
    catalog.addProduct(shirt);
    state.replaceCatalog(catalog);
    state.addCustomer(
        Customer("alice", ContactDetails("Анна", "anna@example.org", "+375291234567"), 0));
    state.addCustomer(
        Customer("bob", ContactDetails("Борис", "boris@example.org", "+375291234568"), 0));
    auto addresses = state.addresses("alice");
    addresses.add("home", Address("BY", "220002", "Минск", "Лесная", "2", "3"));
    state.replaceAddresses(addresses);
    state.changePrice("SHIRT-BLUE", Money(1199));
    state.changePrice("SHIRT-RED", Money(1299));
    const TimeWindow window(0, std::numeric_limits<Timestamp>::max());
    state.addPromotion(Promotion("ten-percent", "10 процентов", PromotionKind::Percentage, 1000,
                                 Money(1000), window));
    state.addPromotion(Promotion("welcome", "Приветственная", PromotionKind::FixedAmount, 200,
                                 Money(1000), window, {}, false));
    state.registerCoupon(Coupon("WELCOME", "welcome", 100, 1, window));
    StockReceipt receipt("initial-receipt", "Учебный поставщик", "warehouse");
    receipt.addLine(ReceiptLine("SHIRT-BLUE", 20, Money(500), "blue-batch"));
    receipt.addLine(ReceiptLine("SHIRT-RED", 20, Money(500), "red-batch"));
    state.postReceipt(receipt);
    return state;
}
std::string demoScript() {
    return R"(as customer alice
cart add SHIRT-BLUE 3
order preview standard home
order place demo-place
order pay order-1 3738 card no demo-decline "Учебный отказ"
order pay order-1 3738 card yes demo-pay
as operator
fulfill start order-1 demo-start
fulfill pick order-1 SHIRT-BLUE 3 demo-pick
fulfill pack order-1 demo-pack
fulfill ship order-1 TRACK-DEMO demo-ship
as customer alice
order deliver order-1 demo-deliver
return open order-1 demo-open SHIRT-BLUE 1 changed-mind
as operator
return inspect return-7 demo-inspect SHIRT-BLUE 1 1 1 opened
return complete return-7 demo-complete
as customer alice
review add order-1 shirt 5 "Понятный учебный пример"
as operator
review moderate review-10 yes
clock advance 1
report 0 1001
quit
)";
}
} // namespace shop::ui
