#pragma once

#include "post/exceptions/PostalException.hpp"

namespace post {

class IncompatibleServiceException : public PostalException {
public:
    explicit IncompatibleServiceException(const std::string& message) : PostalException(message) {}
};

} // namespace post
