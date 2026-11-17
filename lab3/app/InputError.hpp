#pragma once

#include <stdexcept>

namespace shop::ui {
/** InputError.
 * Описывает ошибку команды или аргумента пользователю консоли.
 */
class InputError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
} // namespace shop::ui
