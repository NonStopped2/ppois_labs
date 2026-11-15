#pragma once

#include "post/exceptions/PostalException.hpp"

namespace post {

// Ошибка суммы или денежной арифметики; исходные Money остаются неизменными.
class InvalidAmountException : public PostalException {
public:
    explicit InvalidAmountException(const std::string& message) : PostalException(message) {}
};

} // namespace post
