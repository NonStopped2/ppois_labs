#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class PromotionRuleException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
