#include "Application.hpp"
#include "Display.hpp"
#include "post/shipments/Letter.hpp"
#include "post/shipments/Parcel.hpp"
#include <algorithm>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace post::cli {
namespace {
// Сообщение об отсутствующем ID полезнее технического map::at; const сохраняется при выводе типа.
template <class Map> auto& lookup(Map& objects, const std::string& id) {
    const auto found = objects.find(id);
    if (found == objects.end()) {
        throw std::invalid_argument("Не найден ID: " + id);
    }
    return found->second;
}
// Проверяется до начала сложного процесса, а не после потери результата emplace.
template <class Map> void requireNew(const Map& objects, const std::string& id) {
    if (id.empty() || objects.contains(id)) {
        throw std::invalid_argument("Пустой или повторный ID: " + id);
    }
}
} // namespace

// Декларативная таблица меню; здесь нет предметных правил или произвольной смены статуса.
const std::array<Application::Menu, 9> Application::menus_{
    {{{"Добавить клиента", &Application::createCustomer},
      {"Список клиентов", &Application::listCustomers},
      {"Демонстрационные данные", &Application::demoData}},
     {{"Оформить письмо", &Application::createLetter},
      {"Оформить посылку", &Application::createParcel},
      {"Все отправления", &Application::listShipments},
      {"История по номеру", &Application::history},
      {"Отменить черновик", &Application::cancelShipment},
      {"Изменить массу/услуги черновика", &Application::reviseShipment}},
     {{"Рассчитать и выставить счёт", &Application::quote},
      {"Счета и платежи", &Application::listInvoices},
      {"Оплатить", &Application::pay},
      {"Вернуть оплату", &Application::refund},
      {"Отменить счёт без удержанных денег", &Application::cancelInvoice}},
     {{"Принять отправление", &Application::accept},
      {"Склад и хранение", &Application::warehouse},
      {"Выдать отправление", &Application::issue},
      {"Разрешение представителю", &Application::authorizePickup},
      {"Отозвать разрешение", &Application::revokeAuthorization}},
     {{"Сформировать мешок, партию и план рейса", &Application::prepareTrip},
      {"Выпустить рейс", &Application::departTrip},
      {"Прибыть в следующий пункт", &Application::arriveTrip},
      {"Выгрузить партии", &Application::unloadTrip},
      {"Обновить допуск машины", &Application::inspectVehicle},
      {"Рейсы, машины и ведомости", &Application::listTransport}},
     {{"Спланировать доставку", &Application::planDelivery},
      {"Начать маршрут", &Application::startDelivery},
      {"Записать попытку", &Application::attemptDelivery},
      {"Завершить и вернуть неудачный груз", &Application::finishDelivery},
      {"Назначить повтор", &Application::retryDelivery},
      {"Маршруты и задания", &Application::listDelivery}},
     {{"Подать запрос возврата", &Application::submitReturn},
      {"Решение по возврату", &Application::decideReturn},
      {"Начать одобренный возврат", &Application::startReturn},
      {"Открыть розыск", &Application::openSearch},
      {"Проверить отделение", &Application::checkSearch},
      {"Завершить розыск", &Application::resolveSearch},
      {"Состояние обращений", &Application::listRequests}},
     {{"Подать требование компенсации", &Application::submitClaim},
      {"Решение по компенсации", &Application::decideClaim},
      {"Выплатить компенсацию", &Application::payClaim},
      {"Состояние кассы", &Application::showCash},
      {"Открыть/закрыть кассу", &Application::manageCash}},
     {{"Настройки и текущее время", &Application::configuration},
      {"Перевести время вперёд", &Application::advanceTime},
      {"Открыть новые смены после окончания старых", &Application::renewShifts},
      {"Добавить перерыв смены", &Application::addBreak}}}};
// Бизнес-объекты строятся из явной учебной конфигурации, не из тестовых Fixtures.
Application::Application(Console& console)
    : console_(console), calculator_(defaultCalculator()), policy_(defaultStoragePolicy()) {
    initialize();
}
// Все начальники и курьеры принадлежат своим отделениям; у касс явные начальные остатки.
void Application::initialize() {
    for (const auto& [id, prefix] :
         std::array{std::pair{"A", "22"}, std::pair{"B", "23"}, std::pair{"C", "24"}}) {
        auto branch = defaultBranch(id, prefix);
        CashRegister cash("cash-" + std::string(id), id);
        cash.open(Money(kOpeningCash), branch, manager(branch).id(), now_);
        const auto& courier = branch.employees().at(1);
        couriers_.emplace(courier.id(), Courier(courier, "bike-" + std::string(id), kMaxWeight));
        cash_.emplace(id, std::move(cash));
        branches_.emplace(id, std::move(branch));
    }
    vehicles_.emplace("van-1",
                      Vehicle("van-1", "BY-1001", kVehicleWeight, kVehicleBags, VehicleType::Van));
    VehicleInspection inspection("inspection-van-1", vehicles_.at("van-1"), now_,
                                 now_ + kInspectionLifetime);
    inspection.approve(manager(branches_.at("A")), now_);
    inspections_.emplace("van-1", std::move(inspection));
}
// Возврат в меню после ошибки не завершает программу и не сбрасывает прежний сеанс.
int Application::run(bool demo) {
    console_.out() << "Почта — учебный CLI. Данные хранятся только в памяти.\n";
    try {
        if (demo) {
            execute(&Application::demoData);
        }
        while (const auto action = selectAction()) {
            try {
                execute(*action);
            } catch (const std::ios_base::failure&) {
                throw;
            } catch (const std::exception& error) {
                console_.out() << "Ошибка операции: " << error.what() << '\n';
            }
        }
        console_.out() << "Сеанс завершён.\n";
        return 0;
    } catch (const std::ios_base::failure& error) {
        if (console_.ended()) {
            console_.out() << "\nВвод завершён. Незавершённая команда отменена.\n";
            return 0;
        }
        console_.out() << "Ошибка ввода-вывода: " << error.what() << '\n';
        return 1;
    }
}
// Ноль в подменю возвращает назад; выбор проверяется до обращения к таблице.
std::optional<Application::Action> Application::selectAction() {
    for (;;) {
        console_.out()
            << "\n1 Клиенты | 2 Отправления | 3 Стоимость/оплата | 4 Отделение\n5 Перевозка | 6 "
               "Курьер | 7 Возврат/розыск | 8 Компенсации/касса | 9 Время/настройки | 0 Выход\n";
        const auto section =
            console_.integer("Раздел", 0, static_cast<std::int64_t>(menus_.size()));
        if (section == 0) {
            return std::nullopt;
        }
        const auto& menu = menus_[static_cast<std::size_t>(section - 1)];
        for (std::size_t index = 0; index < menu.size(); ++index) {
            console_.out() << index + 1 << ' ' << menu[index].first << '\n';
        }
        const auto command =
            console_.integer("Действие (0 — назад)", 0, static_cast<std::int64_t>(menu.size()));
        if (command != 0) {
            return menu[static_cast<std::size_t>(command - 1)].second;
        }
    }
}
// Копия включает реестр, кассы и планы: поздняя ошибка не сохраняет даже новый счётчик ID.
void Application::execute(Action action) {
    Application staged = *this;
    (staged.*action)();
    swapState(staged);
}
// Все обмены не бросают: фиксация не может оборваться между реестром и остальными контейнерами.
void Application::swapState(Application& other) noexcept {
    static_assert(std::is_nothrow_swappable_v<ShipmentRegistry> &&
                  std::is_nothrow_swappable_v<PostageCalculator> &&
                  std::is_nothrow_swappable_v<StoragePolicy>);
    using std::swap;
    swap(now_, other.now_);
    swap(nextId_, other.nextId_);
    swap(registry_, other.registry_);
    customers_.swap(other.customers_);
    branches_.swap(other.branches_);
    swap(calculator_, other.calculator_);
    swap(policy_, other.policy_);
    invoices_.swap(other.invoices_);
    vehicles_.swap(other.vehicles_);
    inspections_.swap(other.inspections_);
    trips_.swap(other.trips_);
    manifests_.swap(other.manifests_);
    couriers_.swap(other.couriers_);
    deliveries_.swap(other.deliveries_);
    authorizations_.swap(other.authorizations_);
    returns_.swap(other.returns_);
    searches_.swap(other.searches_);
    claims_.swap(other.claims_);
    cash_.swap(other.cash_);
}
// Служебная последовательность не переполняется и не заменяет пользовательские ID объектов.
std::string Application::nextId(const std::string& prefix) {
    if (nextId_ == std::numeric_limits<std::int64_t>::max()) {
        throw std::overflow_error("Исчерпаны ID сеанса");
    }
    return prefix + "-" + std::to_string(nextId_++);
}
// Поиск всегда выполняется заново: предыдущая успешная команда могла заменить снимки.
PostalBranch& Application::readBranch(std::string_view prompt) {
    return lookup(branches_, console_.text(prompt));
}
// Номер проверяет единственный владелец отправлений.
Shipment& Application::readShipment() {
    return registry_.at(console_.text("Трек-номер"));
}
// Возвращает каноническую карточку, а не созданный снаружи объект с нужной ролью.
const Employee& Application::manager(const PostalBranch& branch) {
    const auto found =
        std::find_if(branch.employees().begin(), branch.employees().end(),
                     [](const auto& employee) { return employee.role() == EmployeeRole::Manager; });
    if (found == branch.employees().end()) {
        throw std::invalid_argument("В отделении нет руководителя");
    }
    return *found;
}
// Документ вводится явно: имя и срок не подставляются так, чтобы автоматически пройти проверку.
IdentityDocument Application::readDocument() {
    const auto kind = console_.integer("Документ: 1 паспорт, 2 ID-карта", 1, 2);
    const auto number = console_.text("Номер документа");
    const auto holder = console_.text("Имя держателя документа");
    const auto issued = console_.date("Дата выдачи");
    const auto expires = console_.date("Дата окончания");
    return IdentityDocument(kind == 1 ? DocumentKind::Passport : DocumentKind::IdentityCard, number,
                            holder, issued, expires);
}
// Промежуточные переменные сохраняют однозначный порядок запросов к пользователю.
Dimensions Application::readDimensions() {
    const auto length = console_.integer("Длина [мм]", 1);
    const auto width = console_.integer("Ширина [мм]", 1);
    const auto height = console_.integer("Высота [мм]", 1);
    return Dimensions(length, width, height);
}
// Проверка личности входит в регистрацию; запрещённые комбинации отвергает DeliveryOptions.
DeliveryOptions Application::readOptions(ShipmentKind kind) {
    const bool registered = console_.yesNo("Заказное");
    const auto declared = console_.money("Объявленная ценность");
    const bool courier = kind == ShipmentKind::Parcel && console_.yesNo("Курьерская доставка");
    const bool fragile = kind == ShipmentKind::Parcel && console_.yesNo("Хрупкое");
    const bool notify = console_.yesNo("Уведомление");
    return DeliveryOptions(registered, declared, registered, courier, fragile, notify);
}
// Пустой элемент и дубли не исчезают молча при разборе списка.
std::vector<std::string> Application::readIds(const std::string& prompt) {
    const auto line = console_.text(prompt + " [через запятую]");
    std::vector<std::string> result;
    std::size_t start = 0;
    do {
        const auto end = line.find(',', start);
        const auto id = trim(
            std::string_view(line).substr(start, end == std::string::npos ? end : end - start));
        if (id.empty() || std::find(result.begin(), result.end(), id) != result.end()) {
            throw std::invalid_argument("Пустой или повторный элемент списка");
        }
        result.push_back(id);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    } while (true);
    return result;
}
// Смещения задаются от текущего времени; абсолютные даты не могут переполниться.
DeliveryTask::Window Application::readWindow() {
    const auto remaining = (kSessionEnd - now_).count();
    const auto from = console_.integer("Начало окна через [с]", 0, remaining);
    const auto to = console_.integer("Конец окна через [с]", 0, remaining);
    return {now_ + std::chrono::seconds{from}, now_ + std::chrono::seconds{to}};
}
// Пустое поле означает личное получение, а не произвольное разрешение по умолчанию.
const PickupAuthorization* Application::readAuthorization() {
    const auto id = console_.text("ID разрешения (пусто — без представителя)");
    return id.empty() ? nullptr : &lookup(authorizations_, id);
}
// Примеры добавляются только при отсутствии конфликтов и никогда не очищают пользовательские
// данные.
void Application::demoData() {
    const ContactDetails contacts("+375291234567");
    for (const auto& [id, name, postcode] :
         std::array{std::tuple{"sender", "Анна Отправитель", "220000"},
                    std::tuple{"recipient", "Борис Получатель", "230000"},
                    std::tuple{"local", "Вера Местная", "220000"},
                    std::tuple{"rep", "Глеб Представитель", "240000"}}) {
        requireNew(customers_, id);
        customers_.emplace(
            id, Customer(id, name, PostalAddress(postcode, "Город", "Почтовая", "1"), contacts));
    }
    const Letter letter(customers_.at("sender"), customers_.at("recipient"), 20, DeliveryOptions{},
                        Dimensions(200, 100, 5), 2, "Документы");
    Parcel parcel(customers_.at("sender"), customers_.at("local"), 500,
                  DeliveryOptions(false, Money{}, false, true), Dimensions(200, 100, 50),
                  PackagingKind::Box);
    parcel.addItem(ContentItem("Книга", 1, 100, Money(1000), ContentCategory::GeneralGoods));
    parcel.seal(nextId("seal"));
    const auto first = registry_.registerShipment(letter, kMaxWeight, sizeLimit(), kMaxUnits);
    const auto second = registry_.registerShipment(parcel, kMaxWeight, sizeLimit(), kMaxUnits);
    console_.out() << "Демо: sender, recipient, local, rep; черновики " << first << ", " << second
                   << ".\n";
}
// Адрес и контакты проходят те же конструкторы, что и в библиотеке.
void Application::createCustomer() {
    const auto id = console_.text("ID клиента");
    requireNew(customers_, id);
    const auto name = console_.text("Полное имя");
    const auto postcode = console_.text("Индекс");
    const auto locality = console_.text("Населённый пункт");
    const auto street = console_.text("Улица");
    const auto building = console_.text("Дом");
    const auto apartment = console_.text("Квартира (пусто — нет)");
    const auto phone = console_.text("Телефон");
    const auto email = console_.text("Email (пусто — нет)");
    const PostalAddress address(postcode, locality, street, building,
                                apartment.empty() ? std::nullopt : std::optional{apartment});
    const ContactDetails contacts(phone, email.empty() ? std::nullopt : std::optional{email});
    customers_.emplace(id, Customer(id, name, address, contacts));
    console_.out() << "Клиент создан: " << id << '\n';
}
// Вывод не раскрывает изменяемую карточку клиента.
void Application::listCustomers() {
    for (const auto& [id, customer] : customers_) {
        const auto& address = customer.address();
        console_.out() << id << " | " << customer.fullName() << " | " << address.postalCode() << ' '
                       << address.locality() << ' ' << address.street() << ' ' << address.building()
                       << " | " << customer.contacts().phone() << '\n';
    }
}
// Реестр получает готовый прототип и сам присваивает уникальный номер.
void Application::createLetter() {
    const auto& sender = lookup(customers_, console_.text("ID отправителя"));
    const auto& recipient = lookup(customers_, console_.text("ID получателя"));
    const auto weight = console_.integer("Масса [г]", 1);
    const auto dimensions = readDimensions();
    const auto sheets = console_.integer("Листов", 1, kMaxUnits);
    const auto description = console_.text("Описание");
    const auto options = readOptions(ShipmentKind::Letter);
    const Letter letter(sender, recipient, weight, options, dimensions, sheets, description);
    console_.out() << "Создано отправление "
                   << registry_.registerShipment(letter, kMaxWeight, sizeLimit(), kMaxUnits)
                   << '\n';
}
// Опись и пломба создаются до регистрации: незаконченную посылку нельзя случайно отправить.
void Application::createParcel() {
    const auto& sender = lookup(customers_, console_.text("ID отправителя"));
    const auto& recipient = lookup(customers_, console_.text("ID получателя"));
    const auto weight = console_.integer("Масса брутто [г]", 1);
    const auto dimensions = readDimensions();
    const auto options = readOptions(ShipmentKind::Parcel);
    const auto packaging = console_.integer("Упаковка: 1 коробка, 2 мягкий конверт", 1, 2);
    Parcel parcel(sender, recipient, weight, options, dimensions,
                  packaging == 1 ? PackagingKind::Box : PackagingKind::PaddedEnvelope);
    const auto count = console_.integer("Позиций описи", 1, kMaxItems);
    for (std::int64_t index = 0; index < count; ++index) {
        const auto name = console_.text("Название вложения");
        const auto quantity = console_.integer("Количество", 1, kMaxUnits);
        const auto unitWeight = console_.integer("Масса единицы [г]", 1);
        const auto value = console_.money("Ценность единицы");
        const auto category = console_.integer("Категория: 1 документы, 2 вещи, 3 хрупкое", 1, 3);
        parcel.addItem(ContentItem(name, quantity, unitWeight, value,
                                   static_cast<ContentCategory>(category - 1)));
    }
    parcel.seal(console_.text("Пломба посылки"));
    console_.out() << "Создано отправление "
                   << registry_.registerShipment(parcel, kMaxWeight, sizeLimit(), kMaxUnits)
                   << '\n';
}
// Включаются активные и архивные отправления; перечисляются все известные статусы модели.
void Application::listShipments() {
    for (int value = 0; value <= static_cast<int>(ShipmentStatus::Lost); ++value) {
        for (const auto& shipment : registry_.findByStatus(static_cast<ShipmentStatus>(value))) {
            printShipment(console_.out(), shipment);
        }
    }
}
// История принадлежит отправлению и доступна только для чтения.
void Application::history() {
    printHistory(console_.out(), readShipment());
}
// Отмена допустима только до приёма; меню не меняет статус само.
void Application::cancelShipment() {
    auto& shipment = readShipment();
    const auto reason = console_.text("Причина отмены");
    shipment.cancelBeforeAcceptance(now_, reason);
    printShipment(console_.out(), shipment);
}
// Изменение черновика увеличивает версию: прежнее ценовое предложение нельзя оплатить заново.
void Application::reviseShipment() {
    auto& shipment = readShipment();
    const auto weight = console_.integer("Новая масса [г]", 1);
    const auto options = readOptions(shipment.kind());
    shipment.revise(weight, options);
    console_.out() << "Черновик изменён, версия " << shipment.revision()
                   << ". Прежние предложения устарели.\n";
}
// Каждое предложение связано с конкретной версией отправления и явным сроком.
void Application::quote() {
    const auto& shipment = readShipment();
    const auto id = console_.text("ID нового счёта");
    requireNew(invoices_, id);
    const auto candidates = calculator_.compareAvailablePlans(shipment, "compare", now_);
    for (const auto& candidate : candidates) {
        console_.out() << candidate.tariffPlanId() << " | " << moneyText(candidate.total()) << '\n';
    }
    const auto plan = console_.text("Тариф [standard/express]");
    const auto quote = calculator_.quote(shipment, plan, "quote-" + id, now_);
    auto [position, inserted] = invoices_.emplace(id, Invoice(id, quote, now_, quote.expiresAt()));
    (void)inserted;
    printInvoice(console_.out(), position->second);
}
// Журналы объясняют долг после частичных оплат и возвратов.
void Application::listInvoices() {
    for (const auto& [id, invoice] : invoices_) {
        (void)id;
        printInvoice(console_.out(), invoice);
    }
}
// Финансовую пару фиксирует CashRegister, а не отдельные ручные изменения журнала UI.
void Application::pay() {
    auto& branch = readBranch();
    auto& invoice = lookup(invoices_, console_.text("ID счёта"));
    const auto id = console_.text("ID платежа");
    const auto amount = console_.money("Сумма оплаты");
    const auto method = console_.integer("Способ: 1 наличные, 2 карта", 1, 2);
    const Payment payment(id, invoice.id(), amount,
                          method == 1 ? PaymentMethod::Cash : PaymentMethod::Card, now_);
    lookup(cash_, branch.id())
        .acceptPayment(invoice, payment, registry_.at(invoice.quote().shipmentTrackingNumber()),
                       branch, manager(branch).id());
    printInvoice(console_.out(), invoice);
}
// Возврат ссылается на исходную оплату; компенсация оформляется другим разделом.
void Application::refund() {
    auto& branch = readBranch();
    auto& invoice = lookup(invoices_, console_.text("ID счёта"));
    const auto payment = console_.text("ID исходного платежа");
    const auto id = console_.text("ID возврата оплаты");
    const auto amount = console_.money("Сумма возврата");
    const auto reason = console_.text("Причина");
    const Refund refund(id, payment, invoice.id(), amount, now_, reason);
    lookup(cash_, branch.id()).refundPayment(invoice, refund, branch, manager(branch).id());
    printInvoice(console_.out(), invoice);
}
// Счёт с удержанными средствами сначала требует возврата; UI не стирает его журнал.
void Application::cancelInvoice() {
    lookup(invoices_, console_.text("ID счёта")).cancel();
    console_.out() << "Счёт отменён.\n";
}
// Отделение проверяет оплату, адрес отправителя, смену и размещение одной операцией.
void Application::accept() {
    auto& branch = readBranch();
    const auto id = readShipment().trackingNumber();
    const auto& invoice = lookup(invoices_, console_.text("ID счёта"));
    branch.acceptShipment(registry_, invoice, id, manager(branch).id(), policy_, kMaxWeight,
                          sizeLimit(), kMaxUnits, now_);
    printShipment(console_.out(), registry_.at(id));
}
// Плата и срок вычисляются по снимку размещения, а не по придуманной CLI дате.
void Application::warehouse() {
    const auto& branch = readBranch();
    for (const auto& area : branch.storageAreas()) {
        for (const auto& cell : area.cells()) {
            for (const auto& entry : cell.entries()) {
                console_.out() << entry.shipmentId() << " | " << cell.id() << " | до "
                               << timeText(entry.deadlineAt()) << " | хранение "
                               << moneyText(entry.storageFeeUntil(policy_, now_)) << '\n';
            }
        }
    }
}
// Личность и доверенность проходят общую предметную проверку отделения.
void Application::issue() {
    auto& branch = readBranch();
    const auto id = readShipment().trackingNumber();
    const auto& claimant = lookup(customers_, console_.text("ID получателя/представителя"));
    const auto document = readDocument();
    const auto* authorization = readAuthorization();
    branch.issueShipment(registry_, id, manager(branch).id(), claimant, document, now_,
                         authorization);
    printShipment(console_.out(), registry_.at(id));
}
// Получатель при возврате — исходный отправитель, его данные в Shipment не переставляются.
void Application::authorizePickup() {
    const auto id = console_.text("ID разрешения");
    requireNew(authorizations_, id);
    const auto& shipment = readShipment();
    const auto& representative = lookup(customers_, console_.text("ID представителя"));
    const auto valid = console_.integer("Действует ещё [с]", 0, (kSessionEnd - now_).count());
    const auto& principal =
        shipment.status() == ShipmentStatus::Returning ? shipment.sender() : shipment.recipient();
    authorizations_.emplace(id, PickupAuthorization(id, shipment, principal, representative, now_,
                                                    now_ + std::chrono::seconds{valid}));
    console_.out() << "Разрешение создано: " << id << '\n';
}
// Отзыв необратим даже при попытке использовать старую дату операции.
void Application::revokeAuthorization() {
    lookup(authorizations_, console_.text("ID разрешения")).revoke(now_);
    console_.out() << "Разрешение отозвано.\n";
}
// Одна команда готовит согласованные мешок, партию, рейс и ведомость, но ещё не забирает груз.
void Application::prepareTrip() {
    const auto id = console_.text("ID рейса");
    requireNew(trips_, id);
    const auto& source = readBranch("Отделение отправления [A/B/C]");
    const auto& target = readBranch("Следующее отделение [A/B/C]");
    const auto& vehicle = lookup(vehicles_, console_.text("ID машины"));
    const auto ids = readIds("Трек-номера");
    const auto route = postalRoute("route-" + id, {source.id(), target.id()});
    const auto plan = sortingPlan(source.id(), target.id());
    MailBag bag("bag-" + id, vehicle.maxWeightGrams(), target.id());
    for (const auto& number : ids) {
        (void)source.storageEntry(number);
        bag.add(registry_.at(number), target, plan);
    }
    bag.seal("bag-seal-" + id, now_, registry_);
    DispatchBatch batch("batch-" + id, source.id(), target.id(), now_);
    batch.addBag(bag);
    batch.prepareForDispatch(route);
    TransportTrip trip(id, route);
    trip.loadBatch(batch, vehicle);
    const auto manifest = TransportManifest::buildFor("manifest-" + id, trip, now_);
    trips_.emplace(id, std::move(trip));
    manifests_.emplace(id, manifest);
    console_.out() << "План рейса " << id << " готов; обработка до выезда "
                   << std::chrono::duration_cast<std::chrono::seconds>(kHandling).count()
                   << " с.\n";
}
// Выезд повторно проверяет допуск и всю складскую композицию.
void Application::departTrip() {
    auto& trip = lookup(trips_, console_.text("ID рейса"));
    auto& source = lookup(branches_, trip.route().stops().front().branchId());
    auto& vehicle = lookup(vehicles_, trip.vehicleId());
    const auto& target = trip.route().stops().back().branchId();
    const auto plan = sortingPlan(source.id(), target);
    trip.depart(vehicle, lookup(inspections_, vehicle.id()), lookup(manifests_, trip.id()), source,
                registry_, plan, manager(source).id(), now_);
    console_.out() << "Рейс отправлен: " << trip.id() << '\n';
}
// Время дороги и порядок остановок проверяет TransportTrip.
void Application::arriveTrip() {
    auto& trip = lookup(trips_, console_.text("ID рейса"));
    const auto& branch = readBranch();
    trip.arriveAt(branch, now_);
    console_.out() << "Рейс прибыл: " << trip.id() << '\n';
}
// Машина освобождается только после завершённой выгрузки, в той же транзакции UI.
void Application::unloadTrip() {
    auto& trip = lookup(trips_, console_.text("ID рейса"));
    auto& branch = readBranch();
    const auto count = trip.unloadFor(branch, registry_, manager(branch).id(), policy_, now_);
    if (trip.state() == TripState::Completed) {
        lookup(vehicles_, trip.vehicleId()).releaseFrom(trip);
    }
    console_.out() << "Выгружено партий: " << count << '\n';
}
// Явно подтверждается учебный осмотр без замечаний; это не реальная экспертиза транспорта.
void Application::inspectVehicle() {
    const auto& branch = readBranch();
    const auto& vehicle = lookup(vehicles_, console_.text("ID машины"));
    branch.authorize(manager(branch).id(), BranchOperation::InspectVehicle, now_);
    if (!console_.yesNo("Осмотр без неустранённых замечаний подтверждён")) {
        throw std::invalid_argument("Без подтверждения допуск не выдаётся");
    }
    VehicleInspection inspection(nextId("inspection"), vehicle, now_,
                                 std::min(now_ + kInspectionLifetime, kSessionEnd));
    inspection.approve(manager(branch), now_);
    inspections_.insert_or_assign(vehicle.id(), std::move(inspection));
    console_.out() << "Допуск обновлён.\n";
}
// Ведомость остаётся снимком исходной массы даже после законной выгрузки.
void Application::listTransport() {
    for (const auto& [id, vehicle] : vehicles_) {
        console_.out() << id << " | назначение " << vehicle.assignedTripId().value_or("свободна")
                       << " | допуск до " << timeText(inspections_.at(id).expiresAt()) << '\n';
    }
    for (const auto& [id, trip] : trips_) {
        const auto& manifest = manifests_.at(id);
        console_.out() << id << " | state=" << statusText(trip.state()) << " | "
                       << trip.route().stops().front().branchId() << " -> "
                       << trip.route().stops().back().branchId() << " | ведомость " << manifest.id()
                       << " | " << manifest.declaredWeightGrams() << " г\n";
    }
}
// Все задания планируются на общих копиях: отказ последнего не резервирует первые.
void Application::planDelivery() {
    const auto id = console_.text("ID маршрута доставки");
    requireNew(deliveries_, id);
    const auto& branch = readBranch();
    auto& courier = lookup(couriers_, "courier-" + branch.id());
    const auto ids = readIds("Трек-номера");
    const auto window = readWindow();
    DeliveryRoute route(id, courier.employee().id());
    for (const auto& number : ids) {
        DeliveryTask task(id + "-" + number, registry_.at(number), branch.storageEntry(number));
        task.schedule(window, courier, registry_.at(number), policy_);
        courier.assignTask(task, route);
    }
    deliveries_.emplace(id, std::move(route));
    console_.out() << "Маршрут создан: " << id << "; ID заданий: маршрут-трек-номер.\n";
}
// При старте груз действительно покидает склад, а не только меняет признак задания.
void Application::startDelivery() {
    auto& route = lookup(deliveries_, console_.text("ID маршрута доставки"));
    auto& courier = lookup(couriers_, route.courierId());
    auto& branch = lookup(branches_, courier.employee().branchId());
    courier.startRoute(route, branch, registry_, now_);
    console_.out() << "Курьерский маршрут начат.\n";
}
// Успешное вручение требует документа; неудача требует конкретной причины и пояснения.
void Application::attemptDelivery() {
    auto& route = lookup(deliveries_, console_.text("ID маршрута доставки"));
    const auto task = console_.text("ID задания");
    const auto id = console_.text("ID попытки");
    const bool delivered = console_.yesNo("Вручено");
    const auto& courier = lookup(couriers_, route.courierId());
    const auto& branch = lookup(branches_, courier.employee().branchId());
    if (delivered) {
        const auto& claimant = lookup(customers_, console_.text("ID получателя/представителя"));
        const auto document = readDocument();
        const auto* authorization = readAuthorization();
        route.completeDelivery(task, registry_, courier, branch,
                               DeliveryAttempt(id, now_, DeliveryOutcome::Delivered), policy_,
                               claimant, document, authorization);
    } else {
        const auto reason = console_.integer(
            "Причина: 1 нет дома, 2 адрес не найден, 3 отказ, 4 повреждение", 1, 4);
        const auto note = console_.text("Пояснение");
        route.recordAttempt(task, registry_, courier, branch,
                            DeliveryAttempt(id, now_, DeliveryOutcome::Failed,
                                            static_cast<DeliveryFailureReason>(reason), note),
                            policy_);
    }
    console_.out() << "Попытка записана: " << id << '\n';
}
// Недоставленное возвращается физически; общая нехватка места откатывает весь финиш.
void Application::finishDelivery() {
    auto& route = lookup(deliveries_, console_.text("ID маршрута доставки"));
    auto& courier = lookup(couriers_, route.courierId());
    auto& branch = lookup(branches_, courier.employee().branchId());
    courier.finishRoute(route, branch, registry_, policy_, now_);
    console_.out() << "Курьерский маршрут завершён.\n";
}
// Используется рабочая копия старого задания с журналом, а не новый объект с нулём попыток.
void Application::retryDelivery() {
    const auto& previous = lookup(deliveries_, console_.text("ID прежнего маршрута"));
    const auto taskId = console_.text("ID задания");
    const auto found = std::find_if(previous.tasks().begin(), previous.tasks().end(),
                                    [&taskId](const auto& task) { return task.id() == taskId; });
    if (found == previous.tasks().end()) {
        throw std::invalid_argument("Не найдено задание");
    }
    auto task = *found;
    const auto id = console_.text("ID нового маршрута");
    requireNew(deliveries_, id);
    auto& courier = lookup(couriers_, previous.courierId());
    const auto window = readWindow();
    task.reschedule(window, courier, registry_.at(task.shipmentId()), policy_);
    DeliveryRoute route(id, courier.employee().id());
    courier.assignTask(task, route);
    deliveries_.emplace(id, std::move(route));
    console_.out() << "Повтор назначен: " << id << "; ID задания сохранён: " << taskId << '\n';
}
// Старые завершённые маршруты сохраняются для наблюдения, но не становятся новым журналом попыток.
void Application::listDelivery() {
    for (const auto& [id, route] : deliveries_) {
        console_.out() << id << " | " << route.courierId()
                       << " | начат=" << route.startedAt().has_value()
                       << " завершён=" << route.finishedAt().has_value() << '\n';
        for (const auto& task : route.tasks()) {
            console_.out() << task.id() << " | state=" << statusText(task.state())
                           << " | попыток=" << task.attempts().size() << " | до "
                           << timeText(task.storageDeadlineAt()) << '\n';
        }
    }
}
// Инициатор и основание проверяются до записи запроса; просрочка не подменяет запрос клиента.
void Application::submitReturn() {
    const auto id = console_.text("ID запроса возврата");
    requireNew(returns_, id);
    const auto& shipment = readShipment();
    const auto reason =
        console_.integer("Основание: 1 отправитель, 2 отказ получателя, 3 срок хранения", 1, 3);
    ReturnRequest request(id);
    if (reason == 3) {
        const auto& branch = readBranch();
        request.submit(shipment, manager(branch), branch, ReturnReason::StorageExpired, now_);
    } else {
        const auto& customer = lookup(customers_, console_.text("ID заявителя"));
        request.submit(shipment, customer,
                       reason == 1 ? ReturnReason::SenderRequest : ReturnReason::RecipientRefused,
                       now_);
    }
    returns_.emplace(id, std::move(request));
    console_.out() << "Запрос возврата подан: " << id << '\n';
}
// Решение не начинает физическое перемещение автоматически.
void Application::decideReturn() {
    auto& request = lookup(returns_, console_.text("ID запроса возврата"));
    const auto& branch = readBranch();
    const auto& shipment = registry_.at(request.shipmentId());
    if (console_.yesNo("Одобрить")) {
        request.approve(shipment, manager(branch), branch, policy_, now_);
    } else {
        const auto reason = console_.text("Причина отказа");
        request.reject(shipment, manager(branch), branch, reason, now_);
    }
    console_.out() << "Решение по возврату записано.\n";
}
// После начала используются обычные мешки и рейсы в сторону отправителя.
void Application::startReturn() {
    auto& request = lookup(returns_, console_.text("ID запроса возврата"));
    auto& branch = readBranch();
    request.beginReturn(branch, registry_, manager(branch), now_);
    console_.out() << "Возврат начат.\n";
}
// Полный ожидаемый путь задаёт оператор; каждый его пункт должен быть известным отделением.
void Application::openSearch() {
    const auto id = console_.text("ID розыска");
    requireNew(searches_, id);
    const auto& shipment = readShipment();
    const auto& applicant = lookup(customers_, console_.text("ID заявителя"));
    const auto ids = readIds("Ожидаемые отделения");
    for (const auto& branch : ids) {
        (void)lookup(branches_, branch);
    }
    const auto route = postalRoute("search-route-" + id, ids);
    SearchRequest search(id);
    search.open(shipment, applicant, route, now_);
    searches_.emplace(id, std::move(search));
    console_.out() << "Розыск открыт: " << id << '\n';
}
// Результат физической проверки вводится явно, а не выводится из возможной устаревшей ячейки.
void Application::checkSearch() {
    auto& search = lookup(searches_, console_.text("ID розыска"));
    const auto& branch = readBranch();
    const bool found = console_.yesNo("Физически найдено");
    const auto note = console_.text("Результат проверки");
    search.recordBranchCheck(branch, manager(branch).id(), found, note, now_);
    console_.out() << "Проверка записана.\n";
}
// Утрату нельзя подтвердить без полного розыска и минимального ожидания.
void Application::resolveSearch() {
    auto& search = lookup(searches_, console_.text("ID розыска"));
    auto& branch = readBranch();
    if (console_.yesNo("Найдено (0 — подтвердить утрату)")) {
        search.resolveFound(registry_.at(search.shipmentId()), branch, now_);
    } else {
        search.closeAsLost(branch, registry_, manager(branch).id(), kMinimumSearchAge, now_);
    }
    console_.out() << "Розыск завершён.\n";
}
// Компенсация отдельна от возврата тарифа и имеет собственные доказательства.
void Application::submitClaim() {
    const auto id = console_.text("ID требования компенсации");
    requireNew(claims_, id);
    const auto& shipment = readShipment();
    const auto& claimant = lookup(customers_, console_.text("ID заявителя"));
    const auto amount = console_.money("Запрошенная сумма");
    const auto reason = console_.integer("Основание: 1 утрата, 2 повреждение", 1, 2);
    const auto evidence = readIds("Ссылки на доказательства");
    CompensationClaim claim(id);
    claim.submit(shipment, claimant, amount,
                 reason == 1 ? CompensationReason::Lost : CompensationReason::Damaged, evidence,
                 now_);
    claims_.emplace(id, std::move(claim));
    console_.out() << "Требование подано.\n";
}
// Явный предел не является официальным законом; сотрудник задаёт его для учебного решения.
void Application::decideClaim() {
    auto& claim = lookup(claims_, console_.text("ID требования компенсации"));
    const auto& branch = readBranch();
    if (console_.yesNo("Одобрить")) {
        const auto amount = console_.money("Одобренная сумма");
        const auto limit = console_.money("Предельная сумма по учебному правилу");
        const auto id = console_.text("ID розыска (пусто — повреждение)");
        const auto* search = id.empty() ? nullptr : &lookup(searches_, id);
        const auto& shipment = registry_.at(claim.shipmentId());
        console_.out() << "Оценка: " << moneyText(claim.assess(shipment, limit, search)) << '\n';
        claim.approve(shipment, branch, manager(branch).id(), amount, limit, now_, search);
    } else {
        const auto reason = console_.text("Причина отказа");
        claim.reject(branch, manager(branch).id(), reason, now_);
    }
    console_.out() << "Решение по компенсации записано.\n";
}
// Касса проверяет однократность канонического требования и достаточность наличности.
void Application::payClaim() {
    auto& branch = readBranch();
    auto& claim = lookup(claims_, console_.text("ID требования компенсации"));
    lookup(cash_, branch.id()).payCompensation(claim, branch, manager(branch).id(), now_);
    console_.out() << "Компенсация выплачена.\n";
}
// Числовые стадии сопровождаются типом объекта и его идентификаторами.
void Application::listRequests() {
    for (const auto& [id, request] : returns_) {
        console_.out() << "Возврат " << id << " | " << request.shipmentId()
                       << " | state=" << statusText(request.status()) << '\n';
    }
    for (const auto& [id, search] : searches_) {
        console_.out() << "Розыск " << id << " | " << search.shipmentId()
                       << " | state=" << statusText(search.status())
                       << " | проверено=" << search.checkedBranchIds().size() << '\n';
    }
    for (const auto& [id, claim] : claims_) {
        console_.out() << "Компенсация " << id << " | " << claim.shipmentId()
                       << " | state=" << statusText(claim.status()) << " | одобрено "
                       << moneyText(claim.approvedAmount()) << '\n';
    }
}
// Наличные и карточные журналы выводятся раздельно от текущего остатка.
void Application::showCash() {
    const auto& branch = readBranch();
    const auto& cash = lookup(cash_, branch.id());
    console_.out() << cash.id() << " | открыта=" << cash.isOpen() << " | наличность "
                   << moneyText(cash.cashBalance()) << '\n';
    for (const auto& payment : cash.payments()) {
        console_.out() << payment.id() << " | "
                       << (payment.method() == PaymentMethod::Cash ? "cash" : "card") << " | "
                       << moneyText(payment.amount()) << '\n';
    }
    for (const auto& returned : cash.refunds()) {
        console_.out() << "Возврат " << returned.id() << " | " << moneyText(returned.amount())
                       << '\n';
    }
    for (const auto& [id, entry] : cash.compensations()) {
        console_.out() << "Компенсация " << id << " | " << moneyText(entry.first) << '\n';
    }
}
// Неверный пересчёт наличности не закрывает кассу.
void Application::manageCash() {
    const auto& branch = readBranch();
    auto& cash = lookup(cash_, branch.id());
    const bool open = console_.yesNo("Открыть (0 — закрыть)");
    const auto amount = console_.money(open ? "Начальный остаток" : "Фактически пересчитано");
    if (open) {
        cash.open(amount, branch, manager(branch).id(), now_);
    } else {
        cash.close(amount, branch, manager(branch).id(), now_);
    }
    console_.out() << "Состояние кассы изменено.\n";
}
// Учебное время только растёт и ограничено календарём конфигурации, без ожидания реальных часов.
void Application::advanceTime() {
    const auto delta = console_.integer("Перевести вперёд на [с]", 0, (kSessionEnd - now_).count());
    now_ += std::chrono::seconds{delta};
    console_.out() << "Время: " << timeText(now_) << '\n';
}
// Старые смены закрываются на их плановом конце; действующую нельзя молча заменить новой.
void Application::renewShifts() {
    auto& branch = readBranch();
    const auto previous = branch.workShifts();
    for (const auto& shift : previous) {
        if (shift.state() != ShiftState::Active) {
            continue;
        }
        if (now_ < shift.scheduledEnd()) {
            throw std::invalid_argument("Старая смена ещё действует");
        }
        branch.finishShift(shift.id(), shift.scheduledEnd());
    }
    for (const auto& employee : branch.employees()) {
        WorkShift shift(nextId("shift"), employee.id(), branch.id(), now_,
                        std::min(now_ + kShiftDuration, kSessionEnd));
        branch.addShift(shift);
        branch.startShift(shift.id(), now_);
    }
    console_.out() << "Новые смены открыты.\n";
}
// Перерыв начинает действовать сейчас; полномочия проверяются при каждой последующей операции.
void Application::addBreak() {
    auto& branch = readBranch();
    const auto id = console_.text("ID смены");
    const auto duration =
        console_.integer("Длительность перерыва [с]", 1, (kSessionEnd - now_).count());
    branch.addShiftBreak(id, now_, now_ + std::chrono::seconds{duration});
    console_.out() << "Перерыв записан.\n";
}
// Начальные данные и ограничения видимы пользователю, а не спрятаны в реализации меню.
void Application::configuration() {
    console_.out()
        << "Время: " << timeText(now_)
        << "\nA: 22xxxx, B: 23xxxx, C: 24xxxx; тарифы standard/express; машина van-1.\nЛимит "
        << kMaxWeight
        << " г; хранение 7 дней, бесплатно 2, затем 0.50 BYN/день; попыток 3, интервал 1 "
           "час.\nДорога 1200 с, обработка 300 с; срок предложения 24 часа; розыск минимум 3600 "
           "с.\n";
    for (const auto& [id, branch] : branches_) {
        console_.out() << "Отделение " << id << '\n';
        for (const auto& shift : branch.workShifts()) {
            console_.out() << shift.id() << " | " << shift.employeeId() << " | до "
                           << timeText(shift.scheduledEnd())
                           << " | работает=" << shift.isWorkingAt(now_) << '\n';
        }
    }
}
} // namespace post::cli
