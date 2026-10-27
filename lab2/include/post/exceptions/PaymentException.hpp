#pragma once
#include "post/exceptions/PostalException.hpp"

namespace post {

// Ошибка начисления, оплаты или связанного возврата, а не обращение к реальному банку.
class PaymentException : public PostalException {
public:
    explicit PaymentException(const std::string& message) : PostalException(message) {}
};

} // namespace post
