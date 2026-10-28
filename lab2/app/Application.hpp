#pragma once
#include "Console.hpp"
#include "Defaults.hpp"
#include "post/delivery/Courier.hpp"
#include "post/payments/CashRegister.hpp"
#include "post/returns/ReturnRequest.hpp"
#include "post/transport/TransportManifest.hpp"
#include <array>
#include <map>
#include <optional>
#include <utility>

namespace post::cli {
// Только управление сеансом и перевод пользовательского ввода в вызовы модели.
// Команда выполняется на полной копии состояния: ошибка/EOF не оставляет полупроцесса.
// Это технический UI-класс, не дополнительная предметная сущность.
class Application {
public:
    explicit Application(Console& console);
    int run(bool demo = false);

private:
    using Action = void (Application::*)();
    using Menu = std::vector<std::pair<const char*, Action>>;
    static const std::array<Menu, 9> menus_;
    [[nodiscard]] std::optional<Action> selectAction();
    void execute(Action action);
    void swapState(Application& other) noexcept;
    [[nodiscard]] std::string nextId(const std::string& prefix);
    [[nodiscard]] PostalBranch& readBranch(std::string_view prompt = "Отделение [A/B/C]");
    [[nodiscard]] Shipment& readShipment();
    [[nodiscard]] static const Employee& manager(const PostalBranch& branch);
    [[nodiscard]] IdentityDocument readDocument();
    [[nodiscard]] Dimensions readDimensions();
    [[nodiscard]] DeliveryOptions readOptions(ShipmentKind kind);
    [[nodiscard]] std::vector<std::string> readIds(const std::string& prompt);
    [[nodiscard]] DeliveryTask::Window readWindow();
    [[nodiscard]] const PickupAuthorization* readAuthorization();
    void initialize();
    void demoData();
    void createCustomer();
    void listCustomers();
    void createLetter();
    void createParcel();
    void listShipments();
    void history();
    void cancelShipment();
    void reviseShipment();
    void quote();
    void listInvoices();
    void pay();
    void refund();
    void cancelInvoice();
    void accept();
    void warehouse();
    void issue();
    void authorizePickup();
    void revokeAuthorization();
    [[nodiscard]] DispatchBatch prepareBatch(const std::string& id, const PostalBranch& source,
                                             const PostalBranch& target, const Vehicle& vehicle,
                                             const std::vector<std::string>& ids) const;
    void prepareTrip();
    void departTrip();
    void arriveTrip();
    void unloadTrip();
    void inspectVehicle();
    void listTransport();
    void planDelivery();
    void startDelivery();
    void attemptDelivery();
    void finishDelivery();
    void cancelDelivery();
    void interruptDelivery();
    void retryDelivery();
    void listDelivery();
    void submitReturn();
    void decideReturn();
    void startReturn();
    void openSearch();
    void checkSearch();
    void resolveSearch();
    void submitClaim();
    void decideClaim();
    void payClaim();
    void listRequests();
    void showCash();
    void manageCash();
    void advanceTime();
    void renewShifts();
    void addBreak();
    void configuration();
    void showShifts();

    Console& console_;
    std::chrono::sys_seconds now_{kEpoch};
    std::int64_t nextId_{1};
    ShipmentRegistry registry_{"BY"};
    std::map<std::string, Customer> customers_;
    std::map<std::string, PostalBranch> branches_;
    PostageCalculator calculator_;
    StoragePolicy policy_;
    std::map<std::string, Invoice> invoices_;
    std::map<std::string, Vehicle> vehicles_;
    std::map<std::string, VehicleInspection> inspections_;
    std::map<std::string, TransportTrip> trips_;
    std::map<std::string, TransportManifest> manifests_;
    std::map<std::string, Courier> couriers_;
    std::map<std::string, DeliveryRoute> deliveries_;
    std::map<std::string, PickupAuthorization> authorizations_;
    std::map<std::string, ReturnRequest> returns_;
    std::map<std::string, SearchRequest> searches_;
    std::map<std::string, CompensationClaim> claims_;
    std::map<std::string, CashRegister> cash_;
};
} // namespace post::cli
