#pragma once

#include "shop/returns/InspectionResult.hpp"
#include <map>

namespace shop {
class ReturnRequest;
/** ReturnInspection.
 * Фиксирует осмотр полученного товара. После завершения набор результатов больше не
 * редактируется.
 */
class ReturnInspection {
  public:
    ReturnInspection(std::string_view id, const ReturnRequest& request, Timestamp receivedAt);
    const std::string& id() const noexcept;
    const std::string& requestId() const noexcept;
    Timestamp receivedAt() const noexcept;
    const std::map<std::string, InspectionResult>& results() const noexcept;
    bool finalized() const noexcept;
    void recordResult(const InspectionResult& result, const ReturnRequest& request,
                      const ReturnPolicy& policy);
    void finalize(const ReturnRequest& request);
    bool operator==(const ReturnInspection&) const = default;

  private:
    std::string id_;
    std::string requestId_;
    Timestamp receivedAt_;
    std::map<std::string, InspectionResult> results_;
    bool finalized_ = false;
};
} // namespace shop
