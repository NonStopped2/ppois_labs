#include "post/payments/CashRegister.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/PaymentException.hpp"
#include <algorithm>
#include <type_traits>

namespace post {
// Новая касса закрыта; остаток не задаётся в обход операции открытия.
CashRegister::CashRegister(const std::string& id, const std::string& branchId)
    : id_(detail::identifier(id)), branchId_(detail::identifier(branchId)) {}
// Ключ кассы не равен автоматически идентификатору отделения.
const std::string& CashRegister::id() const noexcept {
    return id_;
}
// Денежные операции ограничены своим отделением.
const std::string& CashRegister::branchId() const noexcept {
    return branchId_;
}
// Остаток начала относится к текущему/последнему сеансу.
const Money& CashRegister::openingBalance() const noexcept {
    return openingBalance_;
}
// Карточные поступления не увеличивают наличность.
const Money& CashRegister::cashBalance() const noexcept {
    return cashBalance_;
}
// Журнал содержит и наличные, и карточные платежи текущего сеанса.
const std::vector<Payment>& CashRegister::payments() const noexcept {
    return payments_;
}
// Метод исходной оплаты определяет влияние возврата на наличность.
const std::vector<Refund>& CashRegister::refunds() const noexcept {
    return refunds_;
}
// Ключ требования предотвращает повтор в одном журнале; статус самого требования сохраняется между
// сеансами.
const CashRegister::CompensationJournal& CashRegister::compensations() const noexcept {
    return compensations_;
}
// Признак не подменяет проверки роли, смены и хронологии.
bool CashRegister::isOpen() const noexcept {
    return isOpen_;
}
// Дата отсутствует, пока касса ни разу не открывалась.
std::optional<std::chrono::sys_seconds> CashRegister::openedAt() const noexcept {
    return openedAt_;
}
// Нужна для запрета повторного открытия раньше фактического закрытия.
std::optional<std::chrono::sys_seconds> CashRegister::closedAt() const noexcept {
    return closedAt_;
}
// Все журналы монотонны; компенсации дополнительно просматриваются по датам, а не по ключу.
std::chrono::sys_seconds CashRegister::latestActivityAt() const noexcept {
    auto latest = std::max(openedAt_.value_or(std::chrono::sys_seconds{}),
                           closedAt_.value_or(std::chrono::sys_seconds{}));
    if (!payments_.empty()) {
        latest = std::max(latest, payments_.back().paidAt());
    }
    if (!refunds_.empty()) {
        latest = std::max(latest, refunds_.back().createdAt());
    }
    for (const auto& [key, entry] : compensations_) {
        (void)key;
        latest = std::max(latest, entry.second);
    }
    return latest;
}
// Операции выполняет работающий сотрудник своей кассы, а не внешняя карточка с нужной ролью.
void CashRegister::requireAccess(const PostalBranch& branch, const std::string& employeeId,
                                 std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    branch.authorize(employeeId, BranchOperation::ManageCash, time);
    if (!isOpen_ || branch.id() != branchId_ || time < latestActivityAt()) {
        throw PaymentException("Cash register closed, foreign or operation backdated");
    }
}
// Новый сеанс начинает новый журнал, но не может предшествовать закрытию старого.
void CashRegister::open(Money opening, const PostalBranch& branch, const std::string& employeeId,
                        std::chrono::sys_seconds time) {
    detail::requireTime(time);
    branch.authorize(employeeId, BranchOperation::ManageCash, time);
    if (isOpen_ || branch.id() != branchId_ || time < latestActivityAt()) {
        throw PaymentException("Cash register cannot open");
    }
    payments_.clear();
    refunds_.clear();
    compensations_.clear();
    openingBalance_ = opening;
    cashBalance_ = opening;
    openedAt_ = time;
    closedAt_.reset();
    isOpen_ = true;
}
// Сначала готовит кассу и счёт: устаревшая версия отправления не должна оставить поступление денег.
void CashRegister::acceptPayment(Invoice& invoice, const Payment& payment, const Shipment& shipment,
                                 const PostalBranch& branch, const std::string& employeeId) {
    requireAccess(branch, employeeId, payment.paidAt());
    if (std::any_of(payments_.begin(), payments_.end(),
                    [&payment](const auto& prior) { return prior.id() == payment.id(); })) {
        throw PaymentException("Duplicate register payment");
    }
    auto staged = *this;
    auto bill = invoice;
    bill.registerPayment(payment, shipment);
    if (payment.method() == PaymentMethod::Cash) {
        staged.cashBalance_ = staged.cashBalance_.add(payment.amount());
    }
    staged.payments_.push_back(payment);
    static_assert(std::is_nothrow_move_assignable_v<Invoice> &&
                  std::is_nothrow_move_assignable_v<CashRegister>);
    invoice = std::move(bill);
    *this = std::move(staged);
}
// Связь и остаток исходной оплаты проверяет счёт; наличная выдача отдельно требует денег в кассе.
void CashRegister::refundPayment(Invoice& invoice, const Refund& refund, const PostalBranch& branch,
                                 const std::string& employeeId) {
    requireAccess(branch, employeeId, refund.createdAt());
    if (std::any_of(refunds_.begin(), refunds_.end(),
                    [&refund](const auto& prior) { return prior.id() == refund.id(); })) {
        throw PaymentException("Duplicate register refund");
    }
    const auto original =
        std::find_if(invoice.payments().begin(), invoice.payments().end(),
                     [&refund](const auto& paid) { return paid.id() == refund.paymentId(); });
    if (original == invoice.payments().end() ||
        (original->method() == PaymentMethod::Cash && refund.amount() > cashBalance_)) {
        throw PaymentException("Refund source absent or insufficient cash");
    }
    auto staged = *this;
    auto bill = invoice;
    bill.registerRefund(refund);
    if (original->method() == PaymentMethod::Cash) {
        staged.cashBalance_ = staged.cashBalance_.subtract(refund.amount());
    }
    staged.refunds_.push_back(refund);
    invoice = std::move(bill);
    *this = std::move(staged);
}
// Компенсация проводится наличными отдельно от возврата тарифа и помечает само требование
// выплаченным.
void CashRegister::payCompensation(CompensationClaim& claim, const PostalBranch& branch,
                                   const std::string& employeeId, std::chrono::sys_seconds time) {
    requireAccess(branch, employeeId, time);
    if (compensations_.contains(claim.id()) || claim.approvedAmount() > cashBalance_) {
        throw PaymentException("Compensation repeated or insufficient cash");
    }
    auto staged = *this;
    auto request = claim;
    request.markPaid(request.approvedAmount(), time);
    staged.cashBalance_ = staged.cashBalance_.subtract(request.approvedAmount());
    staged.compensations_.emplace(request.id(), std::pair{request.approvedAmount(), time});
    static_assert(std::is_nothrow_move_assignable_v<CompensationClaim>);
    claim = std::move(request);
    *this = std::move(staged);
}
// Не представляет знаковую недостачу типом Money: результат лишь совпадение учёта и факта.
bool CashRegister::reconcile(Money countedCash) const noexcept {
    return countedCash == cashBalance_;
}
// Несовпадение наличности или неверная дата сохраняют кассу открытой без изменения журнала.
void CashRegister::close(Money countedCash, const PostalBranch& branch,
                         const std::string& employeeId, std::chrono::sys_seconds time) {
    requireAccess(branch, employeeId, time);
    if (!reconcile(countedCash)) {
        throw PaymentException("Cash reconciliation mismatch");
    }
    closedAt_ = time;
    isOpen_ = false;
}
} // namespace post
