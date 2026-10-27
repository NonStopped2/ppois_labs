# Содержательный аудит поведений lab2

Область: 50 реализованных предметных классов библиотеки `post_domain`.
[Каталог](README.md#4-каталог-предметных-классов) перечисляет 133 операции;
само количество методов не доказывает уникальность предметных поведений.
Здесь используется более осторожный подсчёт: **110 учитываемых операций**,
а 23 кандидата намеренно исключены. Это обоснование модели для проверки,
а не обещание того, как преподаватель оценит каждую спорную операцию.

## Что не увеличивает счётчик

Конструкторы, деструкторы, копирование, сравнение объектов, простые геттеры,
сеттеры, enum, исключения и свободные технические функции не входят даже
в исходные 133 кандидата. Не добавляются к ним закрытые складские передачи,
проверки аргументов, обёртки `DeliveryRoute::recordAttempt/completeDelivery`
и делегирующие операции управления сменами отделения.

Дополнительно исключены следующие **23** кандидата:

| Класс | Исключённые методы | Почему подсчёт можно сделать строже |
|---|---|---|
| `PostalAddress` | `validate` | Проверка карточки уже выполняется при конструировании |
| `ContactDetails` | `validate` | Повторная проверка уже проверенного значения |
| `IdentityDocument` | `isValidOn` | Подпроверка периода входит в проверку держателя |
| `ContentItem` | `totalWeightGrams`, `totalDeclaredValue` | Две похожие операции умножения; не нужны для достижения порога |
| `TrackingEvent` | `validate` | Проверка структуры уже входит в конструирование события |
| `TariffZone` | `validateCoverage`, `covers`, `overlaps` | Общие операции проверки и сопоставления префиксов |
| `WeightBand` | `contains`, `overlaps` | Простые проверки числовых границ |
| `InvoiceLine` | `lineTotal` | Делегирование умножению денег |
| `PostageQuote` | `total` | Агрегация строк без нового правила тарификации |
| `Payment` | `validateFor` | Подпроверка учитываемой регистрации оплаты в счёте |
| `Refund` | `validateAgainst` | Подпроверка учитываемой регистрации возврата в счёте |
| `CashRegister` | `reconcile` | Простое сравнение факта и остатка; учитывается протокол закрытия |
| `StorageEntry` | `isOverdueAt`, `storageFeeUntil` | Сравнение дат и делегирование политике с ограничением срока |
| `RouteStop` | `earliestDeparture` | Проверяемое сложение даты и длительности, сходное с расчётом дедлайна |
| `DeliveryRoute` | `nextPendingTask` | Выбор элемента и продвижение курсора, не отдельная доставка |
| `DeliveryTask` | `completeDelivery` | Правило личности общее с выдачей отделением; не считаем вручение дважды |
| `DeliveryAttempt` | `validateOutcome` | Повтор проверки неизменяемого результата, выполненной конструктором |
| `CompensationClaim` | `markPaid` | Подтверждение стадии учитываемой согласованной выплаты кассой |

Исключённые операции остаются полезной частью API и тестируются. Нулевой
результат класса в таблице ниже не делает его пустышкой: например, `Customer`
владеет проверенными снимками данных, а `TariffZone` задаёт область и коэффициент
тарификации. Искусственные методы ради увеличения счётчика им не добавляются.

## Учитываемые операции

| Класс | Количество | Методы |
|---|---:|---|
| `Money` | 3 | `add`, `subtract`, `multiply` |
| `Dimensions` | 3 | `fitsWithin`, `volumeCubicMm`, `lengthPlusGirthMm` |
| `PostalAddress` | 1 | `sameDeliveryPointAs` |
| `ContactDetails` | 1 | `selectChannel` |
| `IdentityDocument` | 1 | `validateForHolder` |
| `Customer` | 0 | — |
| `Shipment` | 3 | `applyTrackingEvent`, `cancelBeforeAcceptance`, `revise` |
| `Letter` | 1 | `validateForPosting` |
| `Parcel` | 3 | `addItem`, `seal`, `validateForPosting` |
| `ContentItem` | 0 | — |
| `DeliveryOptions` | 2 | `validateCombination`, `requiredServices` |
| `TrackingEvent` | 0 | — |
| `TrackingHistory` | 4 | `append`, `eventsBetween`, `lastKnownBranch`, `timeSpentAt` |
| `TariffZone` | 0 | — |
| `WeightBand` | 1 | `priceFor` |
| `ServiceRate` | 2 | `calculateSurcharge`, `validateApplicability` |
| `TariffPlan` | 2 | `validateStructure`, `resolveRates` |
| `PostageCalculator` | 2 | `quote`, `compareAvailablePlans` |
| `PostageQuote` | 1 | `validateFor` |
| `InvoiceLine` | 0 | — |
| `Invoice` | 4 | `amountDue`, `registerPayment`, `registerRefund`, `cancel` |
| `Payment` | 0 | — |
| `Refund` | 0 | — |
| `CashRegister` | 5 | `open`, `acceptPayment`, `refundPayment`, `payCompensation`, `close` |
| `PostalBranch` | 4 | `acceptShipment`, `transferOut`, `receiveBatch`, `issueShipment` |
| `StorageArea` | 2 | `planPlacement`, `occupancyByKind` |
| `StorageCell` | 2 | `place`, `take` |
| `StorageEntry` | 0 | — |
| `Employee` | 1 | `authorize` |
| `WorkShift` | 6 | `start`, `addBreak`, `finish`, `workedDuration`, `overlaps`, `isWorkingAt` |
| `SortingRule` | 2 | `matches`, `conflictsWith` |
| `SortingPlan` | 2 | `validateRules`, `groupForDispatch` |
| `MailBag` | 3 | `add`, `remove`, `seal` |
| `DispatchBatch` | 3 | `addBag`, `prepareForDispatch`, `markReceived` |
| `PostalRoute` | 3 | `validateSequence`, `connects`, `estimateDuration` |
| `RouteStop` | 0 | — |
| `Vehicle` | 2 | `assignTo`, `releaseFrom` |
| `VehicleInspection` | 4 | `addDefect`, `resolveDefect`, `approve`, `validateFor` |
| `TransportTrip` | 4 | `loadBatch`, `depart`, `arriveAt`, `unloadFor` |
| `TransportManifest` | 2 | `buildFor`, `verifyAgainst` |
| `Courier` | 3 | `assignTask`, `startRoute`, `finishRoute` |
| `DeliveryRoute` | 3 | `addTask`, `reorder`, `complete` |
| `DeliveryTask` | 3 | `schedule`, `recordAttempt`, `reschedule` |
| `DeliveryAttempt` | 1 | `canRetryAfter` |
| `PickupAuthorization` | 2 | `validateFor`, `revoke` |
| `StoragePolicy` | 3 | `deadlineFrom`, `chargeUntil`, `canAttemptDelivery` |
| `ReturnRequest` | 4 | `submit`, `approve`, `reject`, `beginReturn` |
| `SearchRequest` | 4 | `open`, `recordBranchCheck`, `resolveFound`, `closeAsLost` |
| `CompensationClaim` | 4 | `submit`, `assess`, `approve`, `reject` |
| `ShipmentRegistry` | 4 | `registerShipment`, `findByStatus`, `findByCustomer`, `archiveCompletedBefore` |
| **Итого** | **110** | |

## Почему оставшиеся операции не просто повторяют названия

- `Letter::validateForPosting` проверяет пределы конверта, массы и листов;
  `Parcel::validateForPosting` — опись, суммарное содержимое, упаковку, пломбу и
  хрупкость. Общие унаследованные проверки повторно не учитываются.
- `Invoice::registerPayment/registerRefund` проверяют документ, живую версию
  отправления/исходную оплату и ведут задолженность. Касса дополнительно согласует
  два изменяемых объекта, наличный/карточный способ, остаток, сеанс и кассовый журнал.
  Поэтому это не простые вызовы счёта под другим именем.
- `MailBag::add`, `DispatchBatch::addBag`, `TransportTrip::loadBatch` относятся
  к разным ограничениям состава: направление и открытый мешок; запечатанные мешки
  без повторного груза; затем остановки, вид транспорта и общий ресурс машины.
  Добавление элемента стандартного контейнера само по себе не учитывается.
- `Courier::assignTask` резервирует единственный маршрут и ограничивает весь груз;
  `DeliveryRoute::addTask` согласует владельца, стадию, уникальность ключа и номера.
  Старт и финиш курьера дополнительно согласуют реестр со складом, включая откат
  нескольких отправлений. Это не изменение одного признака маршрута.
- Планирование первого окна и перенос после неудачи различаются: повтор требует
  фактического возврата, допускающей повтор причины, интервала, оставшегося числа
  попыток и сохранения исходного дедлайна.
- Одобрение возврата проверяет хранение, инициатора и основание; решение компенсации —
  доказательства, результат розыска и денежный предел. Это не одинаковые сеттеры статуса.
- Поиск по статусу и по участнику реестра различается предметным предикатом и
  ролью клиента; обе операции охватывают активный и архивный фонды. Консервативное
  объединение этих двух фильтров в одно поведение всё равно оставляет 109 > 100.

Точное назначение методов, ограничения и сценарии ошибок описаны в каталоге,
заголовках и файлах `tests/<модуль>/<Класс>Tests.cpp`. Все перечисленные методы
имеют реализации; постоянные тесты проверяют обычные случаи, границы и отказ без
частичного изменения там, где метод меняет состояние. Покрытие строк само по себе
не является доказательством смысловой уникальности или проверки всех входных данных.
