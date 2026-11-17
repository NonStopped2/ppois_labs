#pragma once

#include <stdexcept>

namespace shop::ui {
class InputError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
} // namespace shop::ui
