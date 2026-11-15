#pragma once

#include "shop/common/ShopException.hpp"

namespace shop {

/** Invalid values or arithmetic outside the supported representation. */
class InvalidDomainDataException : public ShopException {
  public:
    using ShopException::ShopException;
};

} // namespace shop
