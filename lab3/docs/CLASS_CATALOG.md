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
| LoyaltyAccount | customerId, netDeliveredPurchases, processedOperations, qualifyingSpend | recordDelivery, recordReturn; полный откат и идемпотентность | Money |
| LoyaltyPolicy | tierThresholds, tierDiscountBps, maximumDiscountBps | determineTier, calculateDiscount, nextTierProgress | Money, LoyaltyAccount |
| PriceEntry | sku, unitPrice, changedAt, revision | Неизменяемая запись текущей цены | Money |
| PriceList | entries, revision, minimumUnitPrice | changePrice, applyCategoryPriceChange, removePriceForArchivedVariant | PriceEntry, ProductCatalog, Money |
| DiscountAllocation | sku, quantity, grossTotal, discountTotal | netForUnitRange; точная стоимость любой части единиц | Money |
| QuoteLine | sku, title, quantity, unitPrice, allocation, netTotal, packageSnapshot, returnEligible | Неизменяемая строка предложения; цена, характеристики, упаковка и возвратность | ProductVariant, Money, DiscountAllocation |
| Promotion | id, name, kind, value, minimumSubtotal, window, categoryIds, automatic, enabled | eligibleLines, calculateDiscount, assertApplicable | TimeWindow, Money, QuoteLine, ProductCatalog |
| Coupon | code, promotionId, totalUseLimit, perCustomerLimit, window, enabled | normalizeCode, assertRedeemable | TimeWindow, Customer |
| CouponBook | coupons, heldUses, confirmedUses | registerCode, reserveUse, confirmUse, releaseUse, disableCode, usageSummary | Coupon, Promotion, Customer |

Итого на этом этапе: **21 класс, 91 собственное поле**.
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
- `DuplicateOperationException` — повтор операции с иными данными.
- `PromotionRuleException` — неприменимая акция, недоступный/исчерпанный купон.

Все эти типы покрыты тестами реальных ошибок, а не только тестом конструктора.
