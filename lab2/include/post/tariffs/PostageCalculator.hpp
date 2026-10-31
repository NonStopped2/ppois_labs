#pragma once
#include "post/tariffs/PostageQuote.hpp"
#include "post/tariffs/TariffPlan.hpp"

namespace post {

// PostageCalculator — расчёт стоимости зарегистрированного черновика по TariffPlan.
// Собирает основную пересылку с зональным коэффициентом и доплаты в строки PostageQuote.
// Учитывает округление и срок предложения, сравнивает планы; не меняет отправление и кассу.
class PostageCalculator {
public:
    PostageCalculator(std::vector<TariffPlan> tariffPlans, std::int64_t roundingStepKopecks,
                      std::chrono::seconds quoteLifetime);
    [[nodiscard]] const std::vector<TariffPlan>& tariffPlans() const noexcept;
    [[nodiscard]] std::int64_t roundingStepKopecks() const noexcept;
    [[nodiscard]] std::chrono::seconds quoteLifetime() const noexcept;
    [[nodiscard]] PostageQuote quote(const Shipment& shipment, const std::string& tariffPlanId,
                                     const std::string& quoteId,
                                     std::chrono::sys_seconds time) const;
    [[nodiscard]] std::vector<PostageQuote>
    compareAvailablePlans(const Shipment& shipment, const std::string& quotePrefix,
                          std::chrono::sys_seconds time) const;

private:
    std::vector<TariffPlan> tariffPlans_;
    std::int64_t roundingStepKopecks_;
    std::chrono::seconds quoteLifetime_;
};

} // namespace post
