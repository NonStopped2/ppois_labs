#pragma once

#include <optional>
#include <string>

namespace post {

// Адрес доставки внутри Беларуси: проверяет состав и сравнивает точки доставки.
// Индекс хранится строкой ради ведущих нулей, отсутствие квартиры допустимо.
class PostalAddress {
public:
    PostalAddress(const std::string& postalCode, const std::string& locality,
                  const std::string& street, const std::string& building,
                  std::optional<std::string> apartment = std::nullopt);

    [[nodiscard]] const std::string& postalCode() const noexcept;
    [[nodiscard]] const std::string& locality() const noexcept;
    [[nodiscard]] const std::string& street() const noexcept;
    [[nodiscard]] const std::string& building() const noexcept;
    [[nodiscard]] const std::optional<std::string>& apartment() const noexcept;

    void validate() const;
    [[nodiscard]] bool sameDeliveryPointAs(const PostalAddress& other) const noexcept;
    bool operator==(const PostalAddress& other) const = default;

private:
    std::string postalCode_;
    std::string locality_;
    std::string street_;
    std::string building_;
    std::optional<std::string> apartment_;
};

} // namespace post
