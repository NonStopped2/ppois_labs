#pragma once
#include "shop/common/Address.hpp"
#include <string>
#include <string_view>

namespace shop {
class Warehouse {
  public:
    Warehouse(std::string_view id, std::string_view name, Address address,
              bool acceptingReceipts = true);
    const std::string& id() const noexcept;
    const std::string& name() const noexcept;
    const Address& address() const noexcept;
    bool acceptingReceipts() const noexcept;
    void setAcceptingReceipts(bool enabled) noexcept;
    void assertMayReceive() const;
    bool operator==(const Warehouse&) const = default;

  private:
    std::string id_;
    std::string name_;
    Address address_;
    bool acceptingReceipts_;
};
} // namespace shop
