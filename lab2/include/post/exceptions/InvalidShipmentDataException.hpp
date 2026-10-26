#pragma once

#include "post/exceptions/PostalException.hpp"

namespace post {

class InvalidShipmentDataException : public PostalException {
public:
    explicit InvalidShipmentDataException(const std::string& message) : PostalException(message) {}
};

} // namespace post
