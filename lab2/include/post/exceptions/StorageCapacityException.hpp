#pragma once
#include "post/exceptions/PostalException.hpp"

namespace post {
// Отправление невозможно разместить без нарушения ограничений места или совместимости.
class StorageCapacityException : public PostalException {
public:
    explicit StorageCapacityException(const std::string& message) : PostalException(message) {}
};
} // namespace post
