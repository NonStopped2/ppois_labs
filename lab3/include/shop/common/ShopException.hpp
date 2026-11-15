#pragma once

#include <stdexcept>

namespace shop {

/** Base for explained failures of store operations. */
class ShopException : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

} // namespace shop
