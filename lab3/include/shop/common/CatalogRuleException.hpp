#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class CatalogRuleException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
