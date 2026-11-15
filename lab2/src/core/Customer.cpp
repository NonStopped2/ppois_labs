#include "post/core/Customer.hpp"

#include "post/detail/Text.hpp"

#include <stdexcept>

// Customer — карточка клиента: идентификатор, имя, PostalAddress и ContactDetails.
// Один клиент может быть отправителем, получателем или представителем в разных процессах.
// Данные хранятся по значению: Shipment получает независимый снимок карточки.
namespace post {

// Создаёт карточку с собственными копиями адреса и контактов, не ссылаясь на чужие объекты.
Customer::Customer(const std::string& id, const std::string& fullName, const PostalAddress& address,
                   const ContactDetails& contacts)
    : id_(detail::normalizeText(id)), fullName_(detail::normalizeText(fullName)), address_(address),
      contacts_(contacts) {
    if (!detail::hasPrintableText(id_) || id_.find(' ') != std::string::npos ||
        !detail::hasPrintableText(fullName_)) {
        throw std::invalid_argument("Customer needs an identifier without spaces and a full name");
    }
    // Контейнеры и строки сами управляют памятью: особый деструктор и копирование не нужны.
}

// Идентификатор для связи записей клиента; чтение не даёт возможности заменить ключ.
const std::string& Customer::id() const noexcept {
    return id_;
}

// Нормализованное имя для оформления отправлений и сопоставления с документом.
const std::string& Customer::fullName() const noexcept {
    return fullName_;
}

// Даёт доступ к собственному адресу клиента без копирования и без права его изменить.
const PostalAddress& Customer::address() const noexcept {
    return address_;
}

// Контакты для выбора уведомления; ссылка const сохраняет проверенное состояние карточки.
const ContactDetails& Customer::contacts() const noexcept {
    return contacts_;
}

} // namespace post
