#include "post/transport/TransportManifest.hpp"
#include "post/detail/Cargo.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/ManifestMismatchException.hpp"
#include <algorithm>
#include <iterator>

namespace post {
namespace {
// Порядок партий входит в снимок загрузки.
std::vector<std::string> batchKeys(const TransportTrip& trip) {
    std::vector<std::string> keys;
    std::transform(trip.batches().begin(), trip.batches().end(), std::back_inserter(keys),
                   [](const auto& batch) { return batch.id(); });
    return keys;
}
// Пломбы индексируются по уникальным в рейсе мешкам.
std::map<std::string, std::string> sealSnapshot(const TransportTrip& trip) {
    std::map<std::string, std::string> result;
    for (const auto& batch : trip.batches()) {
        std::transform(batch.bags().begin(), batch.bags().end(),
                       std::inserter(result, result.end()),
                       [](const auto& bag) { return std::pair{bag.id(), bag.sealId()}; });
    }
    return result;
}
} // namespace
// Создаёт независимые контейнеры после проверки непустого корректного состава.
TransportManifest::TransportManifest(const std::string& id, const TransportTrip& trip,
                                     std::chrono::sys_seconds time)
    : id_(detail::identifier(id)), tripId_(trip.id()), issuedAt_(time), batchIds_(batchKeys(trip)),
      seals_(sealSnapshot(trip)), declaredWeightGrams_(trip.totalWeightGrams()) {
    detail::requireTime(time);
    detail::validateCargo(trip.batches());
    if (trip.batches().empty()) {
        throw ManifestMismatchException("Cannot issue empty manifest");
    }
    for (const auto& batch : trip.batches()) {
        if (time < batch.createdAt() ||
            std::any_of(batch.bags().begin(), batch.bags().end(),
                        [time](const auto& bag) { return time < *bag.sealedAt(); })) {
            throw ManifestMismatchException("Manifest predates cargo");
        }
    }
}
// Фабрика не публикует наполовину заполненную ведомость.
TransportManifest TransportManifest::buildFor(const std::string& id, const TransportTrip& trip,
                                              std::chrono::sys_seconds time) {
    return TransportManifest(id, trip, time);
}
// Идентификатор ведомости независим от рейса.
const std::string& TransportManifest::id() const noexcept {
    return id_;
}
// Проверка не ограничивается совпадением списка мешков чужого рейса.
const std::string& TransportManifest::tripId() const noexcept {
    return tripId_;
}
// Дата ограничивает самое раннее фактическое отправление.
std::chrono::sys_seconds TransportManifest::issuedAt() const noexcept {
    return issuedAt_;
}
// Возвращает снимок исходного порядка партий.
const std::vector<std::string>& TransportManifest::batchIds() const noexcept {
    return batchIds_;
}
// Внешний код не может заменить одну пломбу в ведомости.
const std::map<std::string, std::string>& TransportManifest::seals() const noexcept {
    return seals_;
}
// Исходная масса не уменьшается при последующей законной выгрузке.
std::int64_t TransportManifest::declaredWeightGrams() const noexcept {
    return declaredWeightGrams_;
}
// Перемещение и статусы не меняют состав; изменение списка, пломб или массы обнаруживается.
void TransportManifest::verifyAgainst(const TransportTrip& trip) const {
    if (trip.id() != tripId_ || batchKeys(trip) != batchIds_ || sealSnapshot(trip) != seals_ ||
        trip.totalWeightGrams() != declaredWeightGrams_) {
        throw ManifestMismatchException("Trip disagrees with manifest");
    }
}
} // namespace post
