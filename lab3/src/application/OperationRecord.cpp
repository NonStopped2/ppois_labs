#include "shop/application/OperationRecord.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
std::string unusedOperationId(const OperationJournal& journal, std::string_view preferred) {
    const auto base = detail::text(preferred, "ID команды");
    auto candidate = base;
    std::size_t suffix = 0;
    while (journal.contains(candidate)) {
        candidate = base + "-" + std::to_string(++suffix);
    }
    return detail::text(candidate, "ID команды");
}
std::optional<OperationResult> replayOperation(const OperationJournal& journal,
                                               std::string_view operationId,
                                               const CommandSignature& signature) {
    const auto key = detail::text(operationId, "ID команды");
    const auto found = journal.find(key);
    if (found == journal.end()) {
        return std::nullopt;
    }
    detail::check<DuplicateOperationException>(found->second.signature == signature,
                                               "ID команды уже использован с другими данными");
    return found->second.result;
}
void recordOperation(OperationJournal& journal, std::string_view operationId,
                     const CommandSignature& signature, const OperationResult& result) {
    const auto key = detail::text(operationId, "ID команды");
    detail::check<DuplicateOperationException>(
        journal.emplace(key, OperationRecord{signature, result}).second, "Команда уже записана");
}
} // namespace shop
