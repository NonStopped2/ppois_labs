#pragma once
#include "post/exceptions/PostalException.hpp"

namespace post {

// Для адреса, веса, даты или необходимой услуги нет применимого тарифа.
class TariffNotFoundException : public PostalException {
public:
    explicit TariffNotFoundException(const std::string& message) : PostalException(message) {}
};

} // namespace post
