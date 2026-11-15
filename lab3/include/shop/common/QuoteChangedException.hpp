#pragma once
#include "shop/common/ShopException.hpp"
namespace shop {
class QuoteChangedException : public ShopException {
  public:
    using ShopException::ShopException;
};
} // namespace shop
