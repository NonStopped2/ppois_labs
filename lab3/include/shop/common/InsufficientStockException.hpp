#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class InsufficientStockException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
