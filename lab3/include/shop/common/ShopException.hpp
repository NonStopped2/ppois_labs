#pragma once

#include <stdexcept>

namespace shop {

/** Базовое исключение с пояснением отказа операции магазина. */
class ShopException : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

} // namespace shop
