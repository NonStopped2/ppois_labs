#include "Display.hpp"
#include <array>
#include <iomanip>
#include <ostream>
#include <sstream>
namespace post::cli {
// Форматирование целых копеек не использует плавающую точку.
std::string moneyText(Money amount) {
    constexpr auto scale = 100;
    std::ostringstream output;
    output << amount.kopecks() / scale << '.' << std::setw(2) << std::setfill('0')
           << amount.kopecks() % scale << " BYN";
    return output.str();
}
// UTC исключает зависимость демонстрации от часового пояса машины.
std::string timeText(std::chrono::sys_seconds time) {
    const auto days = std::chrono::floor<std::chrono::days>(time);
    const std::chrono::year_month_day date{days};
    const std::chrono::hh_mm_ss clock{time - days};
    std::ostringstream output;
    output << static_cast<int>(date.year()) << '-' << std::setfill('0') << std::setw(2)
           << static_cast<unsigned>(date.month()) << '-' << std::setw(2)
           << static_cast<unsigned>(date.day()) << ' ' << std::setw(2) << clock.hours().count()
           << ':' << std::setw(2) << clock.minutes().count() << ':' << std::setw(2)
           << clock.seconds().count() << " UTC";
    return output.str();
}
// Имена повторяют объявленный порядок enum, неизвестное значение не индексирует массив.
std::string statusText(ShipmentStatus status) {
    static constexpr std::array names{"Draft",          "Accepted",  "PreparedForDispatch",
                                      "InTransit",      "Arrived",   "OutForDelivery",
                                      "AwaitingPickup", "Delivered", "Returning",
                                      "Returned",       "Cancelled", "Lost"};
    const auto index = static_cast<std::size_t>(status);
    return index < names.size() ? names[index] : "Unknown";
}
// Финансовые стадии выводятся именами, чтобы оплаченный счёт не путать с нулём долга после отмены.
std::string statusText(InvoiceStatus status) {
    static constexpr std::array names{"Open",     "PartiallyPaid", "Paid", "PartiallyRefunded",
                                      "Refunded", "Cancelled"};
    return names.at(static_cast<std::size_t>(status));
}
// Последняя физическая остановка отличается от завершённой выгрузки.
std::string statusText(TripState status) {
    static constexpr std::array names{"Planned", "InTransit", "Arrived", "Completed"};
    return names.at(static_cast<std::size_t>(status));
}
// Неудача и фактический возврат курьерского груза — разные стадии.
std::string statusText(DeliveryTaskState status) {
    static constexpr std::array names{"Draft",  "Scheduled",        "InProgress",
                                      "Failed", "ReturnedToBranch", "Delivered"};
    return names.at(static_cast<std::size_t>(status));
}
// Одобренное обращение ещё не означает начатый возврат.
std::string statusText(ReturnStatus status) {
    static constexpr std::array names{"Draft", "Submitted", "Approved", "Rejected", "Started"};
    return names.at(static_cast<std::size_t>(status));
}
// Результат розыска не кодируется неоднозначным числом в выводе.
std::string statusText(SearchStatus status) {
    static constexpr std::array names{"Draft", "Open", "Found", "Lost"};
    return names.at(static_cast<std::size_t>(status));
}
// Решение о компенсации и выплата читаются как разные факты.
std::string statusText(CompensationStatus status) {
    static constexpr std::array names{"Draft", "Submitted", "Approved", "Rejected", "Paid"};
    return names.at(static_cast<std::size_t>(status));
}
// Краткая строка позволяет видеть результат операции без доступа к внутренним полям.
void printShipment(std::ostream& output, const Shipment& shipment) {
    output << shipment.trackingNumber() << " | "
           << (shipment.kind() == ShipmentKind::Letter ? "Letter" : "Parcel") << " | "
           << statusText(shipment.status()) << " | " << shipment.weightGrams() << " г | "
           << shipment.sender().id() << " -> " << shipment.recipient().id() << '\n';
}
// Печатает все факты, а не только последний статус; числовой вид сопровождается описанием.
void printHistory(std::ostream& output, const Shipment& shipment) {
    printShipment(output, shipment);
    static constexpr std::array events{"Accepted",       "PreparedForDispatch", "Dispatched",
                                       "Arrived",        "OutForDelivery",      "AwaitingPickup",
                                       "DeliveryFailed", "Delivered",           "ReturnStarted",
                                       "Returned",       "Cancelled",           "Lost",
                                       "Information"};
    for (const auto& event : shipment.history().events()) {
        output << '#' << event.sequence() << ' ' << timeText(event.occurredAt()) << " | "
               << events.at(static_cast<std::size_t>(event.kind())) << " | "
               << event.branchId().value_or("—") << " | " << event.description() << '\n';
    }
}
// Строки счёта объясняют цену, журналы показывают источники оплат и возвратов.
void printInvoice(std::ostream& output, const Invoice& invoice) {
    output << "Счёт " << invoice.id() << " для " << invoice.quote().shipmentTrackingNumber()
           << " status=" << statusText(invoice.status()) << "\n";
    for (const auto& line : invoice.quote().lines()) {
        output << line.description() << " | " << moneyText(line.lineTotal()) << '\n';
    }
    output << "Итого: " << moneyText(invoice.quote().total())
           << "; долг: " << moneyText(invoice.amountDue()) << "; до "
           << timeText(invoice.expiresAt()) << '\n';
    for (const auto& payment : invoice.payments()) {
        output << "Платёж " << payment.id() << ' ' << moneyText(payment.amount()) << '\n';
    }
    for (const auto& refund : invoice.refunds()) {
        output << "Возврат " << refund.id() << ' ' << moneyText(refund.amount()) << '\n';
    }
}
} // namespace post::cli
