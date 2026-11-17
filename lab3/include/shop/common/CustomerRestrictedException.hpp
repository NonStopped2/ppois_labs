#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class CustomerRestrictedException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
