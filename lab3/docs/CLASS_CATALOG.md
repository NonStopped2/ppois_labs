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
| Warehouse | id, name, address, acceptingReceipts | Режим приёма поставок и идентичность склада | Address |
| StockItem | sku, onHand, reserved, reorderPoint | receive, hold, release, consumeHeld, applyCount | — |
| StockReservation | id, orderId, warehouseId, quantities, createdAt, expiresAt, status, changedAt | confirm, consume, release, expire, extend; монотонность времени | — |
| ReceiptLine | sku, quantity, purchaseUnitCost, batchReference | Неизменяемая строка прихода, безопасная стоимость | Money |
| StockReceipt | id, supplierReference, warehouseId, lines, status | addLine, mergeDuplicateSku, validateForPosting, sameDocument | ReceiptLine |
| Inventory | warehouse, items, reservations, postedReceipts | postReceipt, reserve, confirm, extend, release, expire, consume, restock, applyStockCount, replenishmentNeeds, auditReservations | Warehouse, StockItem, StockReservation, StockReceipt, ProductCatalog |
| CartLine | sku, quantity, addedAt | Неизменяемая позиция без фиксированной цены | — |
| Cart | id, customerId, lines, couponCode, revision | addItem, changeQuantity, removeItem, mergeWishlist, removeUnavailable, replaceVariant, adjustToAvailableStock, assertCheckoutReady | CartLine, ProductCatalog, Wishlist, Inventory |

Итого на этом этапе: **29 классов, 128 собственных полей**.
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
- `InsufficientStockException` — недостаточно свободного остатка либо пересчёт уничтожает резерв.
- `ReservationStateException` — недопустимое состояние/время резерва.
- `CartRuleException` — пустая, чужая, переполненная корзина или недоступный вариант.

Все эти типы покрыты тестами реальных ошибок, а не только тестом конструктора.
