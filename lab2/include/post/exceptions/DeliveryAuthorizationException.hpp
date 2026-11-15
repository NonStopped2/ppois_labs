#pragma once

#include "post/exceptions/PostalException.hpp"

namespace post {

class DeliveryAuthorizationException : public PostalException {
public:
    explicit DeliveryAuthorizationException(const std::string& message)
        : PostalException(message) {}
};

} // namespace post
