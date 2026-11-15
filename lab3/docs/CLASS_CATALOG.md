# Реализованные классы интернет-магазина

Этот каталог описывает существующий код; будущие сущности перечислены отдельно
в DOMAIN.md. Поля считаются один раз по собственным объявлениям, технические
классы и исключения не включены в число предметных классов.

| Класс | Собственные поля | Основные методы / содержательные проверки | Связи |
|---|---|---|---|
| Money | kopecks | checkedAdd, checkedSubtract, multiply, fractionFloor, percentageFloor, format | — |
| Address | country, postalCode, city, street, building, apartment | Нормализация и проверка адреса; собственные неизменяемые строки | — |
| ContactDetails | fullName, email, phone | Нормализация телефона и структурная проверка контактов | — |
| TimeWindow | startsAt, endsAt | contains, overlaps; полуинтервал [start,end) | — |
| Category | id, name, parentId, archived | Узел дерева; родителя и архив меняет только каталог | — |
| ProductAttribute | name, kind, required, allowedValues | accepts; обязательность, тип, перечень разрешённых значений | — |
| ProductVariant | sku, label, attributes, weightGrams, dimensionsMm, saleEnabled | conformsTo, matchesAttributes; положительные масса/габариты | ProductAttribute |
| Product | id, categoryId, title, description, attributes, variants, status | addVariant, replaceAttributeSchema, publish, archive, restoreDraft | ProductAttribute, ProductVariant |
| ProductCatalog | categories, products, skuIndex, revision | addCategory, addProduct, addVariant, publish, archiveProduct, restoreProductDraft, setVariantAvailability, moveCategory, archiveCategory, mergeCategories, bulkMoveProducts, search, findCompatibleVariants, isInCategory | Category, Product, ProductVariant |
| Customer | id, contacts, status, registeredAt, restrictionReason | restrictPurchases, restorePurchases, assertMayCheckout | ContactDetails |
| AddressBook | customerId, addresses, defaultAddressId | add, replace, remove, chooseDefault; корректный default после удаления | Address |
| Wishlist | customerId, productIds | addProduct, removeProduct, removeArchived, createCartSelection | Product, ProductCatalog |

Итого на этом этапе: **12 классов, 47 собственных полей**.
Не все перечисленные методы считаются уникальными предметными поведениями:
арифметика Money, валидация и прямые операции редактирования не используются
для искусственного достижения требования о 100 поведениях.

## Технические типы

`ManualClock` хранит now, продвигается только вперёд и проверяет переполнение.
Свободные функции `detail::text`, `require`, `check`, `get` устраняют повторение
валидации и поиска. Они не подменяют предметные сущности.

## Собственные исключения, уже используемые в коде

- `ShopException` — общий базовый тип.
- `InvalidDomainDataException` — некорректные значения/переполнение.
- `EntityNotFoundException` — неизвестная сущность.
- `DuplicateEntityException` — повтор ID, SKU, характеристики или метки адреса.
- `CatalogRuleException` — недопустимый переход или нарушение дерева/схемы.
- `CustomerRestrictedException` — ограниченные покупки и неверный переход ограничения.

Все эти типы покрыты тестами реальных ошибок, а не только тестом конструктора.
