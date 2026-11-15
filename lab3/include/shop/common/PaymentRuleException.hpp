#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class PaymentRuleException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
