#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class CartRuleException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
