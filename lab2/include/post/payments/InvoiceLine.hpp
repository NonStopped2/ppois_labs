#pragma once
#include "post/core/Money.hpp"
#include "post/shipments/ServiceKind.hpp"

#include <string>

namespace post {

// InvoiceLine — одна строка расчёта PostageQuote: услуга, описание, количество и цена единицы.
// Вычисляет сумму строки через проверяемую арифметику Money, а не через double.
// Строка объясняет начисление за услугу, но не означает поступление платежа.
class InvoiceLine {
public:
    InvoiceLine(ServiceKind service, const std::string& description, std::int64_t quantity,
                Money unitPrice);
    [[nodiscard]] ServiceKind service() const noexcept;
    [[nodiscard]] const std::string& description() const noexcept;
    [[nodiscard]] std::int64_t quantity() const noexcept;
    [[nodiscard]] const Money& unitPrice() const noexcept;
    [[nodiscard]] Money lineTotal() const;
    bool operator==(const InvoiceLine& other) const = default;

private:
    ServiceKind service_;
    std::string description_;
    std::int64_t quantity_;
    Money unitPrice_;
};

} // namespace post
