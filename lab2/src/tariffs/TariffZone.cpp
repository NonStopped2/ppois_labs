#include "post/tariffs/TariffZone.hpp"
#include "post/detail/Validation.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

// TariffZone — область доставки по префиксам почтовых индексов и её коэффициент цены.
// Определяет покрытие адреса и пересечение с другой зоной для проверки TariffPlan.
// Коэффициент задан целыми базисными пунктами: 10000 соответствует множителю 1.
namespace post {
namespace {
constexpr std::size_t kPostalCodeLength = 6;

// Один вложенный префикс уже означает пересечение всех соответствующих индексов.
bool intersect(const std::string& first, const std::string& second) {
    return first.starts_with(second) || second.starts_with(first);
}
} // namespace
// Нормализует текст, затем проверяет покрытие и положительный коэффициент.
TariffZone::TariffZone(const std::string& id, const std::string& name,
                       std::vector<std::string> postalPrefixes, std::int64_t priceFactorBasisPoints)
    : id_(detail::identifier(id)), name_(detail::normalizeText(name)),
      postalPrefixes_(std::move(postalPrefixes)), priceFactorBasisPoints_(priceFactorBasisPoints) {
    for (auto& prefix : postalPrefixes_) {
        prefix = detail::normalizeText(prefix);
    }
    if (!detail::hasPrintableText(name_)) {
        throw std::invalid_argument("Zone name is required");
    }
    validateCoverage();
}
// Идентификатор используется для различения зон в плане.
const std::string& TariffZone::id() const noexcept {
    return id_;
}
// Имя не подменяет технический идентификатор.
const std::string& TariffZone::name() const noexcept {
    return name_;
}
// Ведущие нули сохраняются, потому что префиксы хранятся строками.
const std::vector<std::string>& TariffZone::postalPrefixes() const noexcept {
    return postalPrefixes_;
}
// 10000 базисных пунктов соответствуют коэффициенту один.
std::int64_t TariffZone::priceFactorBasisPoints() const noexcept {
    return priceFactorBasisPoints_;
}
// Запрещает пустое покрытие, нецифровые, повторяющиеся и вложенные префиксы.
void TariffZone::validateCoverage() const {
    if (postalPrefixes_.empty() || priceFactorBasisPoints_ <= 0) {
        throw std::invalid_argument("Invalid tariff coverage or factor");
    }
    for (auto current = postalPrefixes_.begin(); current != postalPrefixes_.end(); ++current) {
        const auto& prefix = *current;
        if (prefix.empty() || prefix.size() > kPostalCodeLength ||
            !std::all_of(prefix.begin(), prefix.end(), detail::isAsciiDigit)) {
            throw std::invalid_argument("Postal prefix must contain one to six ASCII digits");
        }
        if (std::any_of(std::next(current), postalPrefixes_.end(),
                        [&prefix](const auto& other) { return intersect(prefix, other); })) {
            throw std::invalid_argument("Redundant postal prefixes");
        }
    }
}
// Адрес уже проверен PostalAddress; здесь определяется только тарифное покрытие.
bool TariffZone::covers(const PostalAddress& address) const {
    return std::any_of(
        postalPrefixes_.begin(), postalPrefixes_.end(),
        [&address](const auto& prefix) { return address.postalCode().starts_with(prefix); });
}
// Пересечение проверяется по всем парам префиксов, а не только по их равенству.
bool TariffZone::overlaps(const TariffZone& other) const {
    return std::any_of(postalPrefixes_.begin(), postalPrefixes_.end(), [&other](const auto& first) {
        return std::any_of(other.postalPrefixes_.begin(), other.postalPrefixes_.end(),
                           [&first](const auto& second) { return intersect(first, second); });
    });
}

} // namespace post
