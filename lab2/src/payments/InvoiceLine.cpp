#include "post/payments/InvoiceLine.hpp"
#include "post/detail/Text.hpp"
#include "post/exceptions/PaymentException.hpp"

// InvoiceLine — одна строка расчёта PostageQuote: услуга, описание, количество и цена единицы.
// Вычисляет сумму строки через проверяемую арифметику Money, а не через double.
// Строка объясняет начисление за услугу, но не означает поступление платежа.
namespace post {

// Принимает известный вид услуги, положительное количество и непустое описание.
InvoiceLine::InvoiceLine(ServiceKind service, const std::string& description, std::int64_t quantity,
                         Money unitPrice)
    : service_(service), description_(detail::normalizeText(description)), quantity_(quantity),
      unitPrice_(unitPrice) {
    if (service_ < ServiceKind::Registration || service_ > ServiceKind::BasePostage ||
        quantity_ <= 0 || !detail::hasPrintableText(description_)) {
        throw PaymentException("Invalid invoice line service, description or quantity");
    }
}
// Вид услуги позволяет обнаружить повторное начисление в предложении.
ServiceKind InvoiceLine::service() const noexcept {
    return service_;
}
// Описание объясняет человеку, за что начислена строка.
const std::string& InvoiceLine::description() const noexcept {
    return description_;
}
// Количество умножается на цену единицы, не хранится в сумме заранее.
std::int64_t InvoiceLine::quantity() const noexcept {
    return quantity_;
}
// Нулевая цена допустима для явно бесплатной услуги.
const Money& InvoiceLine::unitPrice() const noexcept {
    return unitPrice_;
}
// Денежное переполнение обрабатывается общим контрактом Money.
Money InvoiceLine::lineTotal() const {
    return unitPrice_.multiply(quantity_);
}

} // namespace post
