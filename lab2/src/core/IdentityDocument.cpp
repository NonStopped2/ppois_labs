#include "post/core/IdentityDocument.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"

#include <stdexcept>

// IdentityDocument — предъявленный паспорт или ID-карта: номер, имя и даты действия.
// При выдаче в отделении или курьером проверяет срок и совпадение имени владельца.
// Это проверка введённых данных, а не подлинности документа по государственной базе.
namespace post {

// Проверяет вид, номер, владельца и допустимый период ещё при создании документа.
// Проверка ok() исключает невозможные даты до сравнения их порядка.
IdentityDocument::IdentityDocument(DocumentKind kind, const std::string& number,
                                   const std::string& holderName,
                                   std::chrono::year_month_day issuedOn,
                                   std::chrono::year_month_day expiresOn)
    : kind_(kind), number_(detail::normalizeText(number)),
      holderName_(detail::normalizeText(holderName)), issuedOn_(issuedOn), expiresOn_(expiresOn) {
    if (kind_ != DocumentKind::Passport && kind_ != DocumentKind::IdentityCard) {
        throw std::invalid_argument("Unsupported identity document kind");
    }
    if (!detail::hasPrintableText(number_) || number_.find(' ') != std::string::npos ||
        !detail::hasPrintableText(holderName_)) {
        throw std::invalid_argument("Document number and holder name must be nonempty valid text");
    }
    if (!issuedOn_.ok() || !expiresOn_.ok() || issuedOn_ > expiresOn_) {
        throw std::invalid_argument("Invalid identity document validity period");
    }
}

// Вид предъявленного документа из поддерживаемого набора, а не произвольная строка.
DocumentKind IdentityDocument::kind() const noexcept {
    return kind_;
}

// Номер для записи факта предъявления; не означает проверку по государственному реестру.
const std::string& IdentityDocument::number() const noexcept {
    return number_;
}

// Имя для сопоставления с получателем, уже нормализованное без изменения регистра.
const std::string& IdentityDocument::holderName() const noexcept {
    return holderName_;
}

// Нижняя включённая граница: раньше выдачи документ ещё не действует.
std::chrono::year_month_day IdentityDocument::issuedOn() const noexcept {
    return issuedOn_;
}

// Верхняя включённая граница: документ действителен весь указанный календарный день.
std::chrono::year_month_day IdentityDocument::expiresOn() const noexcept {
    return expiresOn_;
}

// Проверяет действие на переданную дату, не обращаясь к системным часам.
bool IdentityDocument::isValidOn(std::chrono::year_month_day date) const {
    if (!date.ok()) {
        throw std::invalid_argument("Invalid presentation date");
    }
    // Обе граничные календарные даты входят в период действия.
    return issuedOn_ <= date && date <= expiresOn_;
}

// Разрешает предъявление только совпадающему владельцу и в пределах срока действия.
void IdentityDocument::validateForHolder(std::string_view expectedName,
                                         std::chrono::year_month_day date) const {
    if (!isValidOn(date) || detail::normalizeText(expectedName) != holderName_) {
        throw DeliveryAuthorizationException(
            "Document is expired, not yet valid or belongs to another holder");
    }
}

} // namespace post
