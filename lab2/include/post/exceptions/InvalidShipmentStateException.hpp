#pragma once

#include "post/exceptions/PostalException.hpp"

namespace post {

class InvalidShipmentStateException : public PostalException {
public:
    explicit InvalidShipmentStateException(const std::string& message) : PostalException(message) {}
};

} // namespace post
