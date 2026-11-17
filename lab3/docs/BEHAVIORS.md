# Реестр предметных поведений

Реализованы **103 различных предметных контракта**, а не 103 любых методов.
Конструкторы, геттеры, прямые сеттеры, арифметика/форматирование Money, JSON,
CLI-обёртки, технические классы и повторение перехода в сущности/координаторе
не добавляют пункты. Успех и отказ одной оплаты считаются одним поведением.

Имена тестов в таблице — постоянные C++ файлы под `lab3/tests/`; UI/CLI не
подменяют эти unit-проверки. Точный перечень Catch2 cases выводит
`shop_core_tests --list-tests`. Соответствие контрактов проверяется содержательно:
автоматический подсчёт строк таблицы сам по себе не доказывает уникальность.

## Каталог

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B001 | ProductCatalog::addProduct | Категория и глобальная уникальность всех SKU | CatalogTests | product add |
| B002 | ProductCatalog::moveCategory | Перенос ветви без цикла и архивного родителя | CatalogTests | category move |
| B003 | ProductCatalog::archiveCategory | Закрытие только категории без действующих детей/продуктов | CatalogTests | category archive |
| B004 | Product::replaceAttributeSchema | Совместимость новой схемы со всеми вариантами черновика | CatalogTests | product attribute |
| B005 | Product::addVariant | Типы/обязательные значения и уникальная комбинация варианта | CatalogTests | variant add |
| B006 | Product::publish | Публикация подготовленного продукта с доступным SKU | CatalogTests | product publish |
| B007 | Product::archive | Прекращение новых продаж с сохранением истории | CatalogTests | product archive |
| B008 | ProductCatalog::setVariantAvailability | Отключение последнего SKU возвращает продукт в Draft | CatalogTests | variant enable |
| B009 | ProductCatalog::search | Совмещение текста, ветви категорий и характеристик | CatalogTests | catalog search |
| B010 | ProductCatalog::findCompatibleVariants | Частичная конфигурация и доступные совпавшие SKU | CatalogTests | catalog variants |
| B011 | ProductCatalog::bulkMoveProducts | Полный откат группового переноса при поздней ошибке | CatalogTests | product move |
| B012 | PriceList::applyCategoryPriceChange | Пакетный пересчёт цен с округлением и общим откатом | PricingTests | price category |
| B013 | PriceList::removePriceForArchivedVariant | Удаление цены только недоступного новым продажам SKU | PricingTests | API, снятие цены не требуется сквозному CLI |
| B014 | ProductCatalog::mergeCategories | Перенос ветвей/продуктов с проверкой конфликтов детей | CatalogTests | category merge |
| B015 | ProductCatalog::compareProducts | Матрица характеристик, отсутствующих цен и свободных остатков | ReviewTests | catalog compare |

## Покупатели, избранное, отзывы

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B016 | AddressBook::add | Первый уникальный адрес становится default | CustomerTests | address add |
| B017 | AddressBook::remove | Детерминированное сохранение корректного default | CustomerTests | address remove |
| B018 | Wishlist::addProduct | Одна ссылка на опубликованный продукт, не на каждый вариант | CustomerTests | wish add |
| B019 | Wishlist::removeArchived | Очистка пожеланий по актуальной доступности продуктов | CustomerTests | wish purge |
| B020 | Wishlist::createCartSelection | Явное разрешение неоднозначности SKU | CustomerTests | wish to-cart |
| B021 | ReviewBook::addVerifiedReview | Один отзыв при наличии полученных не полностью возвращённых единиц | ReviewTests | review add |
| B022 | ProductReview::edit | Проверка автора, сброс голосов и повторная модерация | ReviewTests | review edit |
| B023 | ReviewBook::moderate | Управление видимостью с причиной и влиянием на рейтинг | ReviewTests | review moderate |
| B024 | ReviewBook::ratingSummary | Распределение/точное среднее только видимых оценок | ReviewTests | review rating |
| B025 | ReviewBook::verifiedReviewsFor | Стабильная сортировка и пагинация видимых отзывов | ReviewTests | review list |

## Скидки и лояльность

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B026 | Promotion::eligibleLines | Область скидки по дереву категорий и времени | PromotionTests | order preview |
| B027 | Promotion::calculateDiscount | Процентная/фиксированная скидка с порогом и верхней границей | PromotionTests | order preview |
| B028 | CouponBook::registerCode | Уникальный нормализованный код и связь с акцией | PromotionTests | coupon add |
| B029 | CouponBook::reserveUse | Глобальная и персональная квоты уже при удержании | PromotionTests | order place |
| B030 | CouponBook::confirmUse | Оплата удержанного применения даже после окончания акции | PromotionTests | order pay |
| B031 | CouponBook::releaseUse | Освобождение только неоплаченного применения | PromotionTests | order cancel |
| B032 | PricingPolicy::selectDiscount | Минимум полного платежа с доставкой и стабильная ничья | QuoteTests | order preview |
| B033 | PricingPolicy::allocateDiscount | Пропорциональное распределение без потери копеек | QuoteTests | order preview |
| B034 | DiscountAllocation::netForUnitRange | Точная стоимость последовательной части единиц | PricingTests | return complete |
| B035 | PricingPolicy::makeQuote | Чистый расчёт предложения и его срока из коммерческих политик | QuoteTests | order preview |
| B036 | LoyaltyAccount::recordDelivery | Однократный учёт именно полученного товарного оборота | PricingTests | order deliver |
| B037 | LoyaltyAccount::recordReturn | Уменьшение оборота только соответствующего заказа | PricingTests | return complete |
| B038 | LoyaltyPolicy::determineTier | Уровень по чистому полученному обороту и порогам | PricingTests | order preview |
| B039 | LoyaltyPolicy::calculateDiscount | Скидка уровня с ограничением максимального процента | PricingTests | order preview |
| B040 | LoyaltyPolicy::nextTierProgress | Остаток до следующего уровня, включая максимальный | PricingTests | API |
| B041 | CouponBook::usageSummary | Раздельные удержанные/оплаченные/доступные применения | PromotionTests | coupon usage |

## Склад

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B042 | StockReceipt::mergeDuplicateSku | Объединение совместимых партий, запрет неоднозначной цены | InventoryTests | API |
| B043 | Inventory::postReceipt | Приход всего документа и сравнение данных при повторе ID | InventoryTests | receipt post |
| B044 | Inventory::reserve | Резерв всех SKU либо ни одного | InventoryTests | order place |
| B045 | Inventory::confirm | Оплаченный резерв больше не истекает по сроку оплаты | InventoryTests | order pay |
| B046 | Inventory::release | Освобождение всего неиспользованного, не отгруженного резерва | InventoryTests | order cancel |
| B047 | Inventory::consume | Однократное списание подтверждённого резерва на отправке | InventoryTests | fulfill ship |
| B048 | Inventory::restock | Приход пригодной части возврата в общей транзакции | InventoryTests, ReturnProcessingTests | return complete |
| B049 | Inventory::applyStockCount | Фактический пересчёт не уничтожает зарезервированные единицы | InventoryTests | stock count |
| B050 | Inventory::replenishmentNeeds | Потребность по точкам пополнения от свободного остатка | InventoryTests | stock needs |
| B051 | Inventory::auditReservations | Сверка SKU, заказов, фаз, сроков и reserved | StoreStateTests | stock audit |

## Корзина и оформление

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B052 | Cart::addItem | Объединение SKU с общим лимитом и контролем переполнения | CartTests | cart add |
| B053 | Cart::changeQuantity | Новый общий объём корзины, не только количество одной строки | CartTests | cart quantity |
| B054 | Cart::mergeWishlist | Атомарное добавление явно выбранных вариантов пожеланий | CartTests | wish to-cart |
| B055 | Cart::removeUnavailable | Удаление недоступных SKU с сохранением остальных | CartTests | cart prune |
| B056 | Cart::replaceVariant | Только вариант того же продукта, объединение количеств и времени | CartTests | cart switch |
| B057 | Cart::adjustToAvailableStock | Явное уменьшение до свободного остатка без резервирования | CartTests | cart adjust |
| B058 | Checkout::preview | Владелец/статус/наличие товара перед чистым денежным расчётом | CheckoutTests | order preview |
| B059 | Checkout::placeOrder | Повторная проверка условий и общий commit заказа/резерва/купона/корзины | CheckoutTests | order place |
| B060 | Checkout::expireUnpaidOrders | Атомарное закрытие одного истёкшего неоплаченного заказа | CheckoutTests | clock cleanup |

## Заказ и оплата

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B061 | OrderBook::customerHistory | Фильтры статуса/периода и устойчивый порядок покупок | OrderTests | order list |
| B062 | OrderProcessing::pay | Проверка владельца/срока/суммы, отказ или общий commit оплаты | ProcessingTests | order pay |
| B063 | OrderProcessing::cancel | Неотправленный заказ: освободить резерв, возместить оплату, остановить подбор | ProcessingTests | order cancel |
| B064 | PaymentBook::refund | Финансовый документ без превышения фактической оплаты | PaymentTests | return complete |
| B065 | PaymentBook::reconcileOrderBalance | Сверка текущего заказа с оплатой/возвратами/историей | PaymentTests | report |
| B066 | OrderBook::findOverdueUnpaid | Только неоплаченные заказы с истёкшим сроком | OrderTests | clock cleanup |
| B067 | OrderProcessing::repeatOrder | Явный актуальный набор SKU без старой цены/купона | ProcessingTests | order repeat |
| B068 | OrderProcessing::extendPaymentDeadline | Одно продление заказа и Held-резерва вместе | ProcessingTests | order extend |
| B069 | OrderProcessing::changeUnpaidDelivery | Подтверждённая новая доставка по неизменным товарным снимкам | ProcessingTests | order delivery-confirm |

## Исполнение и доставка

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B070 | DeliveryPolicy::eligibleOptions | Доступные способы по адресу, массе и размеру | QuoteTests | order preview |
| B071 | DeliveryOption::calculateFee | Порог бесплатной доставки после товарной скидки | QuoteTests | order preview |
| B072 | DeliveryPolicy::estimateArrival | Интервал получения из обработки и перевозки | QuoteTests | API |
| B073 | DeliveryPolicy::cheapestEligible | Минимальная допустимая доставка со стабильной ничьей | QuoteTests | API |
| B074 | OrderProcessing::startPicking | Начало только оплаченной покупки с подтверждённым резервом | ProcessingTests | fulfill start |
| B075 | Fulfillment::pick | Подбор SKU не сверх требуемого количества | FulfillmentTests | fulfill pick |
| B076 | Fulfillment::undoPick | Исправление подбора без отрицательного количества | FulfillmentTests | fulfill undo |
| B077 | Fulfillment::pack | Упаковка только полностью подобранного заказа | FulfillmentTests | fulfill pack |
| B078 | OrderProcessing::dispatch | Уникальный трек, передача и списание резерва вместе | ProcessingTests | fulfill ship |
| B079 | OrderProcessing::confirmDelivery | Получение и товарный оборот лояльности одним commit | ProcessingTests | order deliver |
| B080 | Fulfillment::reopenPicking | Распаковка до передачи с сохранением подбора | FulfillmentTests | fulfill reopen |

## Возвраты

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B081 | ReturnPolicy::assertEligible | Право, причина и срок по условиям покупки | QuoteTests | return open |
| B082 | ReturnProcessing::open | Владелец, остаток права и одна активная заявка | ReturnProcessingTests | return open |
| B083 | ReturnProcessing::withdraw | Отзыв до осмотра освобождает право нового обращения | ReturnProcessingTests | return withdraw |
| B084 | ReturnProcessing::reject | Мотивированный отказ без движения денег/остатка | ReturnProcessingTests | return reject |
| B085 | ReturnInspection::recordResult | SKU заявки, количества, пригодность и запрет дубля | ReturnTests | return inspect |
| B086 | ReturnInspection::finalize | Полнота и полное получение всех запрошенных единиц | ReturnTests | return inspect |
| B087 | ReturnPolicy::classifyRestockability | Принимаемый брак не равен пригодному товару | QuoteTests | return inspect |
| B088 | ReturnPolicy::calculateRefund | Сумма принятого по снимку и предыдущим завершённым возвратам | ReturnTests | return complete |
| B089 | ReturnProcessing::complete | Общий commit денег, пригодного остатка и лояльности, включая ноль денег | ReturnProcessingTests | return complete |
| B090 | ReturnProcessing::amendSubmitted | Новый состав до осмотра без продления срока | ReturnProcessingTests | return amend |

## Отчёты и дополнительные действия

| ID | Метод | Самостоятельный контракт | Unit-тесты | CLI |
|---|---|---|---|---|
| B091 | SalesReport::buildFinancialSummary | Денежные операции периода и отдельная доставка | ReportTests | report |
| B092 | SalesReport::buildSkuSummary | Полученные/возвращённые единицы и товарные суммы одной когорты | ReportTests | report |
| B093 | SalesReport::returnRate | Доля принятых единиц когорты, без деления на ноль | ReportTests | report |
| B094 | SalesReport::averageOrderValue | Чистый товарный чек с полностью возвращёнными заказами в знаменателе | ReportTests | report |
| B095 | SalesReport::reconcile | Историческая сверка документов/событий на end, не равенство разных когорт | ReportTests | report |
| B096 | SalesReport::abcAnalysis | Накопленный вклад, пороги, ничьи и нулевая выручка | ReportTests | report |
| B097 | SalesReport::qualityAnalysis | Обращения, принятый брак, пригодные и отклонённые единицы по дате осмотра | ReportTests | report |
| B098 | SalesReport::repeatPurchaseRate | Доля покупателей с ≥2 сохранившимися полученными покупками | ReportTests | report |
| B099 | Checkout::moveCartItemsToWishlist | Атомарный перенос выбранных SKU с объединением вариантов в продукт | CheckoutTests | cart wish |
| B100 | SalesReport::rfmSegments | Явные пороги давности/частоты/чистой суммы и сегмент без получений | ReportTests | report |
| B101 | SalesReport::promotionUsageAnalysis | Исторические удержания/оплаты/скидки/возвраты когорты заказов | ReportTests | report |
| B102 | PaymentBook::attemptOutcomeAnalysis | Настоящие попытки и оплата после отказов, без повторов ID | PaymentTests | report |
| B103 | ProductReview::voteHelpful | Один голос зарегистрированного неавтора за видимый отзыв | ReviewTests | review vote |

Чистый расчёт B035, проверка возможности покупки B058 и commit B059 имеют разные
контракты. Стоимость единиц B034 и согласование документов B088 тоже не одно действие.
Локальный текущий баланс B065 и историческая междокументная сверка B095 отличаются
областью/границей времени. Ветки одной команды и её CLI-вызов отдельно не посчитаны.

## Типизированные ассоциации

Ниже **52 уникальные направленные пары только по собственным полям**. Пара повторно
не считается, даже если в классе несколько полей Money. Строковые ID, перечисления,
вспомогательные записи, наследование исключений и зависимости технических классов
не увеличивают этот счётчик. Методы дают дополнительные связи, которые здесь не нужны.

| Класс | Типизированные поля / типы элементов |
|---|---|
| Customer | ContactDetails |
| AddressBook | Address |
| LoyaltyAccount | Money |
| LoyaltyPolicy | Money |
| Product | ProductAttribute, ProductVariant |
| ProductCatalog | Category, Product |
| PriceEntry | Money |
| PriceList | PriceEntry, Money |
| Promotion | Money, TimeWindow |
| Coupon | TimeWindow |
| CouponBook | Coupon |
| QuoteLine | Money, DiscountAllocation |
| PriceQuote | QuoteLine, Address, Money, ReturnPolicy |
| DiscountAllocation | Money |
| Warehouse | Address |
| Inventory | Warehouse, StockItem, StockReservation, StockReceipt |
| ReceiptLine | Money |
| StockReceipt | ReceiptLine |
| Cart | CartLine |
| OrderLine | Money, DiscountAllocation |
| Order | OrderLine, Address, Money, OrderEvent, ReturnPolicy |
| OrderBook | Order |
| PaymentAttempt | Money |
| Payment | Money |
| PaymentBook | PaymentAttempt, Payment, Refund |
| Refund | Money |
| DeliveryOption | Money |
| DeliveryPolicy | DeliveryOption |
| ReturnLine | Money |
| ReturnRequest | ReturnLine |
| ReturnInspection | InspectionResult |
| SalesReport | TimeWindow, Money |
| ReviewBook | ProductReview |

Даже без связей с Money остаётся **35 пар**, больше требуемых 30.
