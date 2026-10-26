#pragma once

#include "post/exceptions/PostalException.hpp"

namespace post {

class InvalidAddressException : public PostalException {
public:
    explicit InvalidAddressException(const std::string& message) : PostalException(message) {}
};

} // namespace post
