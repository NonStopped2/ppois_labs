#pragma once

#include <stdexcept>
#include <string>

namespace post {

// Позволяет будущему CLI перехватить все предметные ошибки одним catch.
class PostalException : public std::runtime_error {
public:
    explicit PostalException(const std::string& message) : std::runtime_error(message) {}
};

} // namespace post
