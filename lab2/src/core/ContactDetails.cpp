#include "post/core/ContactDetails.hpp"

#include "post/detail/Text.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>

// ContactDetails — контакты клиента: телефон, необязательный email и предпочтительный канал.
// Выбирает канал из доступных с учётом указанных контактов и предпочтения.
// Проверяет формат, но не существование номера или ящика; сам уведомления не отправляет.
namespace post {
namespace {

constexpr std::size_t kMinimumPhoneDigits = 7;
constexpr std::size_t kMaximumPhoneDigits = 15;
constexpr std::string_view kEmailLocalSymbols = "._%+-";

// Проверяет латинские ASCII-буквы без зависимости от системной локали.
bool isAsciiLetter(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

// Убирает только разрешённые разделители: прочие ошибки должен заметить validatePhone.
std::string normalizePhone(std::string_view phone) {
    std::string normalized;
    normalized.reserve(phone.size());
    std::copy_if(phone.begin(), phone.end(), std::back_inserter(normalized), [](char character) {
        constexpr std::string_view separators = "-()";
        return !detail::isAsciiSpace(character) &&
               separators.find(character) == std::string_view::npos;
    });
    return normalized;
}

// Проверяет учебный международный формат номера, а не его существование у оператора.
void validatePhone(const std::string& phone) {
    // Длина проверяется первой: короткое замыкание || защищает front() и обращение [1].
    if (phone.size() < kMinimumPhoneDigits + 1 || phone.size() > kMaximumPhoneDigits + 1 ||
        phone.front() != '+' || phone[1] == '0') {
        throw std::invalid_argument("Phone must start with '+' and contain 7 to 15 digits");
    }
    if (!std::all_of(phone.begin() + 1, phone.end(), detail::isAsciiDigit)) {
        throw std::invalid_argument("Phone contains an unsupported character");
    }
}

// Разрешает поддерживаемые символы до @, исключая крайние и последовательные точки.
bool validLocalPart(std::string_view localPart) {
    if (localPart.empty() || localPart.front() == '.' || localPart.back() == '.' ||
        localPart.find("..") != std::string_view::npos) {
        return false;
    }
    return std::all_of(localPart.begin(), localPart.end(), [](char character) {
        return isAsciiLetter(character) || detail::isAsciiDigit(character) ||
               kEmailLocalSymbols.find(character) != std::string_view::npos;
    });
}

// Проверяет доменные метки: нужна точка, недопустимы пустые метки и крайние дефисы.
bool validDomain(std::string_view domain) {
    if (domain.empty() || domain.front() == '.' || domain.back() == '.' || domain.front() == '-' ||
        domain.back() == '-' || domain.find('.') == std::string_view::npos ||
        domain.find("..") != std::string_view::npos ||
        domain.find(".-") != std::string_view::npos ||
        domain.find("-.") != std::string_view::npos) {
        return false;
    }
    return std::all_of(domain.begin(), domain.end(), [](char character) {
        return isAsciiLetter(character) || detail::isAsciiDigit(character) || character == '-' ||
               character == '.';
    });
}

// Разделяет адрес с единственным @ и проверяет обе части ограниченного формата.
void validateEmail(const std::string& email) {
    const auto separator = email.find('@');
    if (separator == std::string::npos || separator != email.rfind('@')) {
        throw std::invalid_argument("Email must contain exactly one '@'");
    }
    // string_view позволяет рассматривать части email без копирования строк.
    const std::string_view view(email);
    const auto localPart = view.substr(0, separator);
    const auto domain = view.substr(separator + 1);
    // Это намеренно ограниченный учебный формат, а не полная реализация RFC почты.
    if (!validLocalPart(localPart) || !validDomain(domain)) {
        throw std::invalid_argument("Email must have the form user@host.example");
    }
}

// Отклоняет неизвестные enum во всём списке, даже если подходящий канал уже есть.
void validateChannels(const std::vector<ContactChannel>& channels) {
    const bool hasUnknown =
        std::any_of(channels.begin(), channels.end(), [](ContactChannel channel) {
            return channel != ContactChannel::Phone && channel != ContactChannel::Email;
        });
    if (hasUnknown) {
        throw std::invalid_argument("Unknown available contact channel");
    }
}

} // namespace

// Сохраняет нормализованные контакты и проверяет доступность предпочтительного канала.
ContactDetails::ContactDetails(const std::string& phone, std::optional<std::string> email,
                               ContactChannel preferredChannel)
    : phone_(normalizePhone(phone)), email_(std::move(email)), preferredChannel_(preferredChannel) {
    if (email_) {
        email_ = detail::normalizeText(*email_);
    }
    validate();
}

// Телефон уже приведён к виду '+' и цифры; вызывающему коду не нужна повторная очистка.
const std::string& ContactDetails::phone() const noexcept {
    return phone_;
}

// Отсутствие email явно представлено nullopt, а не пустой строкой.
const std::optional<std::string>& ContactDetails::email() const noexcept {
    return email_;
}

// Предпочтение клиента учитывается первым, но не заменяет проверку доступных каналов.
ContactChannel ContactDetails::preferredChannel() const noexcept {
    return preferredChannel_;
}

// Определяет наличие данных для канала: телефон обязателен, email может отсутствовать.
bool ContactDetails::supports(ContactChannel channel) const {
    switch (channel) {
    case ContactChannel::Phone:
        return true;
    case ContactChannel::Email:
        return email_.has_value();
    }
    throw std::invalid_argument("Unknown contact channel");
}

// Проверяет контакты и не позволяет выбрать предпочтением отсутствующий email.
void ContactDetails::validate() const {
    validatePhone(phone_);
    if (email_) {
        validateEmail(*email_);
    }
    if (!supports(preferredChannel_)) {
        throw std::invalid_argument("Preferred contact channel has no contact details");
    }
}

// Выбирает предпочтительный доступный канал либо первый поддерживаемый резервный.
ContactChannel ContactDetails::selectChannel(const std::vector<ContactChannel>& available) const {
    // Проверяем весь список до выбора: ошибочный enum не должен остаться незамеченным.
    validateChannels(available);
    const auto preferred = std::find(available.begin(), available.end(), preferredChannel_);
    if (preferred != available.end()) {
        return preferredChannel_;
    }
    // Захват this даёт лямбде доступ к контактам текущего объекта через supports.
    const auto fallback =
        std::find_if(available.begin(), available.end(),
                     [this](ContactChannel channel) { return supports(channel); });
    if (fallback != available.end()) {
        return *fallback;
    }
    throw std::invalid_argument("No usable contact channel is available");
}

} // namespace post
