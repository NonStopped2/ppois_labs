#include "shop/application/OperationRecord.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
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
