#pragma once
#include "post/exceptions/PostalException.hpp"
namespace post {
// Ведомость относится к другому рейсу либо больше не описывает его партии, пломбы и массу.
class ManifestMismatchException : public PostalException {
public:
    using PostalException::PostalException;
};
} // namespace post
