#pragma once
#include "shop/common/TimeWindow.hpp"
#include "shop/delivery/DeliveryOption.hpp"
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace shop {
/** DeliveryPolicy.
 * Подбирает допустимые способы доставки и оценивает сроки. Отбор учитывает адрес и
 * сохранённые данные строк.
 */
class DeliveryPolicy {
  public:
    DeliveryPolicy(const std::vector<DeliveryOption>& options, std::int64_t handlingDuration,
                   std::map<std::string, std::pair<std::int64_t, std::int64_t>> transitDurations);
    std::vector<DeliveryOption> eligibleOptions(const Address& destination,
                                                const std::vector<QuoteLine>& lines) const;
    DeliveryOption cheapestEligible(const Address& destination, const std::vector<QuoteLine>& lines,
                                    Money goodsNet) const;
    TimeWindow estimateArrival(std::string_view optionId, Timestamp now) const;
    DeliveryOption option(std::string_view id) const;
    const std::map<std::string, DeliveryOption>& options() const noexcept;
    std::int64_t handlingDuration() const noexcept;
    const std::map<std::string, std::pair<std::int64_t, std::int64_t>>&
    transitDurations() const noexcept;
    bool operator==(const DeliveryPolicy&) const = default;

  private:
    std::map<std::string, DeliveryOption> options_;
    std::int64_t handlingDuration_;
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> transitDurationByOption_;
};
} // namespace shop
