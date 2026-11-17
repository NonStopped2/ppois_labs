#pragma once

#include "shop/common/ShopException.hpp"

namespace shop {

/** Некорректное значение или выход за границы представимых чисел. */
class InvalidDomainDataException : public ShopException {
  public:
    using ShopException::ShopException;
};

} // namespace shop
