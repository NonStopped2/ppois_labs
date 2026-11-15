#pragma once

#include "shop/orders/OrderEvent.hpp"
#include "shop/payments/PaymentBook.hpp"
#include "shop/pricing/PriceQuote.hpp"
#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace shop {
enum class CommandKind {
    PlaceOrder,
    Pay,
    Cancel,
    ExpireOrder,
    StartPicking,
    Pick,
    UndoPick,
    Pack,
    ReopenPicking,
    Dispatch,
    Deliver,
    ExtendPayment,
    ChangeDelivery,
    RepeatOrder,
    MoveToWishlist
};
/** Actual canonical values, not just hashes, protect against operation-ID reuse. */
struct CommandSignature {
    CommandKind kind = CommandKind::PlaceOrder;
    std::string actor;
    std::string entityId;
    std::vector<std::string> texts;
    std::vector<std::int64_t> numbers;
    std::optional<DeliveryTerms> delivery;
    std::optional<PriceQuote> quote;
    bool operator==(const CommandSignature&) const = default;
};
using OperationResult = std::variant<std::monostate, std::string, AttemptOutcome>;
struct OperationRecord {
    CommandSignature signature;
    OperationResult result;
    bool operator==(const OperationRecord&) const = default;
};
using OperationJournal = std::map<std::string, OperationRecord>;
std::optional<OperationResult> replayOperation(const OperationJournal& journal,
                                               std::string_view operationId,
                                               const CommandSignature& signature);
void recordOperation(OperationJournal& journal, std::string_view operationId,
                     const CommandSignature& signature, const OperationResult& result = {});
} // namespace shop
