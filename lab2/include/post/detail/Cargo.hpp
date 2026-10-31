#pragma once
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/sorting/DispatchBatch.hpp"
#include <limits>
#include <set>

namespace post::detail {
// Агрегаты считают исходный состав рейса, включая уже выгруженные партии.
inline std::int64_t cargoWeight(const std::vector<DispatchBatch>& batches) {
    std::int64_t result = 0;
    for (const auto& batch : batches) {
        for (const auto& bag : batch.bags()) {
            if (bag.totalWeightGrams() > std::numeric_limits<std::int64_t>::max() - result) {
                throw std::overflow_error("Cargo mass overflow");
            }
            result += bag.totalWeightGrams();
        }
    }
    return result;
}
// Не сужает size_t без предварительной проверки диапазона.
inline std::int64_t cargoBagCount(const std::vector<DispatchBatch>& batches) {
    std::int64_t result = 0;
    for (const auto& batch : batches) {
        if (batch.bags().size() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - result)) {
            throw std::overflow_error("Bag count overflow");
        }
        result += static_cast<std::int64_t>(batch.bags().size());
    }
    return result;
}
// Проверка состава одного мешка отделена от обхода партий, без четвёртого уровня вложенности.
inline void validateBagCargo(const MailBag& bag, std::set<std::string>& bagIds,
                             std::set<std::string>& sealIds, std::set<std::string>& shipmentIds) {
    if (!bag.sealedAt() || !bagIds.insert(bag.id()).second ||
        !sealIds.insert(bag.sealId()).second) {
        throw InvalidShipmentStateException("Invalid trip bag or seal");
    }
    for (const auto& [number, weight] : bag.shipmentIds()) {
        (void)weight;
        if (!shipmentIds.insert(number).second) {
            throw InvalidShipmentStateException("Cargo appears twice in trip");
        }
    }
}
// Пломба и номер мешка уникальны в рейсе; один груз не входит сразу в две партии.
inline void validateCargo(const std::vector<DispatchBatch>& batches) {
    std::set<std::string> batchIds, bagIds, sealIds, shipmentIds;
    for (const auto& batch : batches) {
        if (!batchIds.insert(batch.id()).second) {
            throw InvalidShipmentStateException("Duplicate trip batch");
        }
        for (const auto& bag : batch.bags()) {
            validateBagCargo(bag, bagIds, sealIds, shipmentIds);
        }
    }
}
} // namespace post::detail
