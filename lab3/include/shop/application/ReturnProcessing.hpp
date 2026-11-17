#pragma once

#include "shop/application/StoreState.hpp"

namespace shop {
/** ReturnProcessing.
 * Согласует заявки, осмотры, деньги, товар и лояльность. Частичный возврат опирается на
 * снимок покупки.
 */
class ReturnProcessing {
  public:
    explicit ReturnProcessing(StoreState& state);
    std::string open(std::string_view actor, std::string_view orderId,
                     const std::vector<ReturnSelection>& selections, std::string_view operationId);
    std::string inspect(std::string_view requestId, const std::vector<InspectionResult>& results,
                        std::string_view operationId);
    void reject(std::string_view requestId, std::string_view reason, std::string_view operationId);
    void withdraw(std::string_view actor, std::string_view requestId, std::string_view operationId);
    Money complete(std::string_view requestId, std::string_view operationId);
    void amendSubmitted(std::string_view actor, std::string_view requestId,
                        const std::vector<ReturnSelection>& selections,
                        std::string_view operationId);
    std::vector<std::string> closeExpiredUnreceived();
    std::map<std::string, std::int64_t> completedQuantities(std::string_view orderId) const;

  private:
    std::vector<ReturnLine> buildLines(const Order& order,
                                       const std::vector<ReturnSelection>& selections) const;
    StoreState& state_;
};
} // namespace shop
