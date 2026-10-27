#pragma once
#include "post/core/Money.hpp"
#include "post/shipments/ServiceKind.hpp"

#include <string>

namespace post {

// Одна прозрачная строка начисления: услуга, описание, количество и цена единицы.
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
