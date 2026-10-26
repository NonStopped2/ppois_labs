#pragma once

#include "post/core/DocumentKind.hpp"

#include <chrono>
#include <string>
#include <string_view>

namespace post {

// Это проверка предъявленных данных, а не подлинности государственного документа.
class IdentityDocument {
public:
    IdentityDocument(DocumentKind kind, const std::string& number, const std::string& holderName,
                     std::chrono::year_month_day issuedOn, std::chrono::year_month_day expiresOn);

    [[nodiscard]] DocumentKind kind() const noexcept;
    [[nodiscard]] const std::string& number() const noexcept;
    [[nodiscard]] const std::string& holderName() const noexcept;
    [[nodiscard]] std::chrono::year_month_day issuedOn() const noexcept;
    [[nodiscard]] std::chrono::year_month_day expiresOn() const noexcept;

    [[nodiscard]] bool isValidOn(std::chrono::year_month_day date) const;
    void validateForHolder(std::string_view expectedName, std::chrono::year_month_day date) const;

    bool operator==(const IdentityDocument& other) const = default;

private:
    DocumentKind kind_;
    std::string number_;
    std::string holderName_;
    std::chrono::year_month_day issuedOn_;
    std::chrono::year_month_day expiresOn_;
};

} // namespace post
