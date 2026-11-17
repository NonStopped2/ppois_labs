#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class ReviewRuleException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
