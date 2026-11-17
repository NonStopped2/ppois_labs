#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class DuplicateEntityException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
