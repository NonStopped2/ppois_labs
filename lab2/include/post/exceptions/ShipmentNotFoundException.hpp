#pragma once
#include "post/exceptions/PostalException.hpp"

namespace post {

// По трек-номеру нет ни активного, ни архивного отправления.
class ShipmentNotFoundException : public PostalException {
public:
    explicit ShipmentNotFoundException(const std::string& message) : PostalException(message) {}
};

} // namespace post
