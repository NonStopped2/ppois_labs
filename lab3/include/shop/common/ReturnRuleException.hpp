#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class ReturnRuleException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
