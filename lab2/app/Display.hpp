#pragma once
#include "post/delivery/DeliveryTaskState.hpp"
#include "post/payments/Invoice.hpp"
#include "post/returns/CompensationStatus.hpp"
#include "post/returns/ReturnStatus.hpp"
#include "post/returns/SearchStatus.hpp"
#include "post/shipments/Shipment.hpp"
#include "post/transport/TransportState.hpp"
#include <iosfwd>
#include <string>
namespace post::cli {
[[nodiscard]] std::string moneyText(Money amount);
[[nodiscard]] std::string timeText(std::chrono::sys_seconds time);
[[nodiscard]] std::string statusText(ShipmentStatus status);
[[nodiscard]] std::string statusText(InvoiceStatus status);
[[nodiscard]] std::string statusText(TripState status);
[[nodiscard]] std::string statusText(DeliveryTaskState status);
[[nodiscard]] std::string statusText(ReturnStatus status);
[[nodiscard]] std::string statusText(SearchStatus status);
[[nodiscard]] std::string statusText(CompensationStatus status);
void printShipment(std::ostream& output, const Shipment& shipment);
void printHistory(std::ostream& output, const Shipment& shipment);
void printInvoice(std::ostream& output, const Invoice& invoice);
} // namespace post::cli
