#pragma once
#include "post/exceptions/PostalException.hpp"

namespace post {

// Нарушение явно заданного ограничения массы, габаритов или количества единиц.
class ShipmentLimitExceededException : public PostalException {
public:
    explicit ShipmentLimitExceededException(const std::string& message)
        : PostalException(message) {}
};

} // namespace post
