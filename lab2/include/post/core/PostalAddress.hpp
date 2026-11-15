#pragma once

#include <optional>
#include <string>

namespace post {

// PostalAddress — адрес клиента для доставки, выбора тарифной зоны и сортировки.
// Хранит шестизначный индекс, населённый пункт, улицу, дом и необязательную квартиру.
// Проверяет формат данных, но не существование адреса; индекс — строка ради ведущих нулей.
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
