#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class FulfillmentStateException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
