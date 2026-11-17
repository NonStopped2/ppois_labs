#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class DuplicateOperationException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
