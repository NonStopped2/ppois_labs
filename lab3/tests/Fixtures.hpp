#pragma once

#include "shop/catalog/ProductCatalog.hpp"
#include "shop/common/Address.hpp"
#include "shop/common/ContactDetails.hpp"
#include "shop/customers/Customer.hpp"

namespace shop::test {

inline Address address() {
    return Address("BY", "220001", "Минск", "Лесная", "1");
}
inline Customer customer(std::string_view id = "alice") {
    return Customer(id, ContactDetails("Анна", "anna@example.org", "+375291234567"), 0);
}
inline Product product(std::string_view id = "shirt", std::string_view sku = "shirt-blue") {
    Product result(id, "clothes", "Футболка", "Хлопок",
                   {ProductAttribute("color", AttributeKind::Text, true, {"blue", "red"})});
    result.addVariant(ProductVariant(sku, "Синяя", {{"color", "blue"}}, 200, {200, 100, 20}));
    return result;
}
inline ProductCatalog catalog() {
    ProductCatalog result;
    result.addCategory(Category("root", "Каталог"));
    result.addCategory(Category("clothes", "Одежда", "root"));
    result.addCategory(Category("other", "Другое", "root"));
    result.addProduct(product());
    result.publish("shirt");
    return result;
}

} // namespace shop::test
