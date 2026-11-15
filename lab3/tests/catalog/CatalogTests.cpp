#include "../Fixtures.hpp"
#include "shop/common/CatalogRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE("Product attributes enforce types choices and required fields") {
    const ProductAttribute integer("size", AttributeKind::Integer, true, {"10", "20"});
    REQUIRE(integer.name() == "size");
    REQUIRE(integer.kind() == AttributeKind::Integer);
    REQUIRE(integer.required());
    REQUIRE(integer.allowedValues().size() == 2);
    REQUIRE(integer.accepts("10"));
    REQUIRE_FALSE(integer.accepts("11"));
    REQUIRE_FALSE(integer.accepts("10x"));
    REQUIRE_FALSE(integer.accepts(""));
    REQUIRE(ProductAttribute("note", AttributeKind::Text, false).accepts(""));
    REQUIRE(ProductAttribute("count", AttributeKind::Integer, true).accepts("-2"));
    REQUIRE_FALSE(
        ProductAttribute("count", AttributeKind::Integer, true).accepts("99999999999999999999"));
    REQUIRE_THROWS_AS(ProductAttribute("x", static_cast<AttributeKind>(9), true),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(ProductAttribute("x", AttributeKind::Integer, true, {"bad"}),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(ProductAttribute("x", AttributeKind::Text, true, {" "}),
                      InvalidDomainDataException);
}

TEST_CASE("Product variants validate physical data and schemas") {
    const ProductVariant variant("sku", "blue", {{"color", "blue"}}, 200, {10, 20, 30});
    REQUIRE(variant.sku() == "sku");
    REQUIRE(variant.label() == "blue");
    REQUIRE(variant.weightGrams() == 200);
    REQUIRE(variant.dimensionsMm()[2] == 30);
    REQUIRE(variant.saleEnabled());
    REQUIRE(variant.matchesAttributes({{"color", "blue"}}));
    REQUIRE_FALSE(variant.matchesAttributes({{"color", "red"}}));
    REQUIRE_FALSE(variant.matchesAttributes({{"other", "blue"}}));
    REQUIRE_FALSE(variant.conformsTo({}));
    REQUIRE_FALSE(variant.conformsTo({ProductAttribute("size", AttributeKind::Integer, true)}));
    REQUIRE(variant.conformsTo({ProductAttribute("color", AttributeKind::Text, true),
                                ProductAttribute("size", AttributeKind::Integer, false)}));
    REQUIRE_THROWS_AS(ProductVariant("s", "v", {}, 0, {1, 1, 1}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(ProductVariant("s", "v", {}, 1, {0, 1, 1}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(ProductVariant("s", "v", {{" color", "blue"}}, 1, {1, 1, 1}),
                      InvalidDomainDataException);
}

TEST_CASE("Products preserve invariants and independent copies") {
    auto product = test::product();
    REQUIRE(product.title() == "Футболка");
    REQUIRE(product.description() == "Хлопок");
    REQUIRE(product.attributes().size() == 1);
    const auto before = product;
    REQUIRE_THROWS_AS(
        product.replaceAttributeSchema({ProductAttribute("size", AttributeKind::Integer, true)}),
        CatalogRuleException);
    REQUIRE(product == before);
    product.replaceAttributeSchema(product.attributes());
    REQUIRE_THROWS_AS(product.addVariant(product.variants().front()), DuplicateEntityException);
    REQUIRE_THROWS_AS(product.addVariant(ProductVariant("bad", "bad", {}, 1, {1, 1, 1})),
                      CatalogRuleException);
    product.publish();
    REQUIRE(product.status() == ProductStatus::Published);
    REQUIRE(before.status() == ProductStatus::Draft);
    REQUIRE_THROWS_AS(product.publish(), CatalogRuleException);
    REQUIRE_THROWS_AS(product.replaceAttributeSchema({}), CatalogRuleException);
    product.archive();
    REQUIRE_THROWS_AS(product.archive(), CatalogRuleException);
    REQUIRE_THROWS_AS(product.addVariant(before.variants().front()), CatalogRuleException);
    product.restoreDraft();
    REQUIRE_THROWS_AS(product.restoreDraft(), CatalogRuleException);
    Product empty("empty", "clothes", "title", "");
    REQUIRE_THROWS_AS(empty.publish(), CatalogRuleException);
    REQUIRE_THROWS_AS(Product("x", "c", "x", "",
                              {ProductAttribute("x", AttributeKind::Text, true),
                               ProductAttribute("x", AttributeKind::Text, true)}),
                      DuplicateEntityException);
}

TEST_CASE("Catalog owns products enforces unique SKU and filters published variants") {
    auto catalog = test::catalog();
    REQUIRE(catalog.revision() == 5);
    REQUIRE(catalog.products().size() == 1);
    REQUIRE(catalog.categories().size() == 3);
    REQUIRE(catalog.category("clothes").parentId() == "root");
    REQUIRE(catalog.search().size() == 1);
    REQUIRE(catalog.search("Хлопок", "root").size() == 1);
    REQUIRE(catalog.search("none").empty());
    REQUIRE(catalog.search("", "other").empty());
    REQUIRE(catalog.search("", "root", {{"color", "red"}}).empty());
    const auto snapshot = catalog.product("shirt");
    const ProductVariant red("shirt-red", "Красная", {{"color", "red"}}, 210, {20, 10, 2});
    catalog.addVariant("shirt", red);
    REQUIRE(snapshot.variants().size() == 1);
    REQUIRE(catalog.variant("shirt-red") == red);
    REQUIRE(catalog.productForSku("shirt-red").id() == "shirt");
    const auto before = catalog;
    REQUIRE_THROWS_AS(catalog.addVariant("shirt", red), DuplicateEntityException);
    REQUIRE_THROWS_AS(catalog.addProduct(test::product("otherProduct")), DuplicateEntityException);
    REQUIRE_THROWS_AS(catalog.addProduct(test::product()), DuplicateEntityException);
    REQUIRE(catalog == before);
    REQUIRE_THROWS_AS(catalog.product("missing"), EntityNotFoundException);
    REQUIRE_THROWS_AS(catalog.variant("missing"), EntityNotFoundException);
    catalog.setVariantAvailability("shirt-blue", false);
    REQUIRE(catalog.findCompatibleVariants("shirt", {}).size() == 1);
    catalog.setVariantAvailability("shirt-red", false);
    REQUIRE(catalog.product("shirt").status() == ProductStatus::Draft);
    REQUIRE(catalog.findCompatibleVariants("shirt", {}).empty());
    catalog.setVariantAvailability("shirt-red", true);
    catalog.publish("shirt");
    catalog.archiveProduct("shirt");
    REQUIRE_THROWS_AS(catalog.setVariantAvailability("shirt-red", true), CatalogRuleException);
    REQUIRE(catalog.search().empty());
    catalog.restoreProductDraft("shirt");
    REQUIRE(catalog.product("shirt").status() == ProductStatus::Draft);
}

TEST_CASE("Category moves reject cycles conflicts and unavailable parents atomically") {
    auto catalog = test::catalog();
    catalog.addCategory(Category("child", "Детская", "clothes"));
    REQUIRE(catalog.isInCategory("child", "root"));
    REQUIRE_FALSE(catalog.isInCategory("other", "clothes"));
    auto before = catalog;
    REQUIRE_THROWS_AS(catalog.moveCategory("root", "child"), CatalogRuleException);
    REQUIRE_THROWS_AS(catalog.archiveCategory("clothes"), CatalogRuleException);
    REQUIRE_THROWS_AS(catalog.addCategory(Category("clothes", "x")), DuplicateEntityException);
    REQUIRE_THROWS_AS(catalog.addCategory(Category("dup", "Одежда", "root")),
                      DuplicateEntityException);
    REQUIRE(catalog == before);
    catalog.moveCategory("child", "other");
    REQUIRE(catalog.category("child").parentId() == "other");
    catalog.moveCategory("child", std::nullopt);
    REQUIRE_FALSE(catalog.category("child").parentId());
    catalog.archiveCategory("child");
    REQUIRE(catalog.category("child").archived());
    REQUIRE_THROWS_AS(catalog.archiveCategory("child"), CatalogRuleException);
    REQUIRE_THROWS_AS(catalog.moveCategory("child", "root"), CatalogRuleException);
    REQUIRE_THROWS_AS(catalog.moveCategory("other", "child"), CatalogRuleException);
    REQUIRE_THROWS_AS(catalog.addCategory(Category("bad", "bad", "child")), CatalogRuleException);
    REQUIRE_THROWS_AS(Category("x", "x", "x"), InvalidDomainDataException);
    catalog.bulkMoveProducts({"shirt"}, "other");
    catalog.archiveCategory("clothes");
    REQUIRE_THROWS_AS(catalog.addProduct(test::product("new", "new-sku")), CatalogRuleException);
}

TEST_CASE("Category merging and bulk moves roll back late failures") {
    auto catalog = test::catalog();
    catalog.addCategory(Category("leftChild", "Общее", "clothes"));
    catalog.addCategory(Category("rightChild", "Общее", "other"));
    const auto before = catalog;
    REQUIRE_THROWS_AS(catalog.mergeCategories("clothes", "other"), DuplicateEntityException);
    REQUIRE_THROWS_AS(catalog.mergeCategories("root", "other"), CatalogRuleException);
    REQUIRE_THROWS_AS(catalog.bulkMoveProducts({"shirt", "missing"}, "other"),
                      EntityNotFoundException);
    REQUIRE_THROWS_AS(catalog.bulkMoveProducts({"shirt", "shirt"}, "other"),
                      DuplicateEntityException);
    REQUIRE_THROWS_AS(catalog.bulkMoveProducts({}, "other"), CatalogRuleException);
    REQUIRE(catalog == before);
    catalog.moveCategory("rightChild", std::nullopt);
    const auto revision = catalog.revision();
    catalog.mergeCategories("clothes", "other");
    REQUIRE(catalog.revision() == revision + 1);
    REQUIRE(catalog.product("shirt").categoryId() == "other");
    REQUIRE(catalog.category("leftChild").parentId() == "other");
    REQUIRE(catalog.category("clothes").archived());
    REQUIRE(catalog.variant("shirt-blue").sku() == "shirt-blue");
    catalog.addCategory(Category("empty", "Пустая", "root"));
    catalog.mergeCategories("empty", "other");
    REQUIRE(catalog.category("empty").archived());
}
