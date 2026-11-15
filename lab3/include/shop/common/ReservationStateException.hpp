#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class ReservationStateException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
