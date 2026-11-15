#pragma once
#include "post/exceptions/PostalException.hpp"
namespace post {
// Маршрут или транспорт не позволяют выполнить требуемую перевозку.
class TransportUnavailableException : public PostalException {
public:
    using PostalException::PostalException;
};
} // namespace post
