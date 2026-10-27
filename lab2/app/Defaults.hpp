#pragma once
#include "post/branches/PostalBranch.hpp"
#include "post/sorting/SortingPlan.hpp"
#include "post/tariffs/PostageCalculator.hpp"
#include "post/transport/PostalRoute.hpp"
#include <chrono>

namespace post::cli {
// Учебные параметры, не официальные ограничения почтового оператора.
inline const std::chrono::sys_seconds kEpoch{
    std::chrono::sys_days{std::chrono::year{2026} / 1 / 1}};
inline const auto kSessionEnd = kEpoch + std::chrono::days{3650};
inline constexpr std::int64_t kMaxWeight = 30000;
inline constexpr std::int64_t kMaxUnits = 1000;
inline constexpr std::int64_t kMaxItems = 100;
inline constexpr std::int64_t kVehicleWeight = 100000;
inline constexpr std::int64_t kVehicleBags = 20;
inline constexpr std::int64_t kOpeningCash = 50000;
inline constexpr auto kShiftDuration = std::chrono::hours{24};
inline constexpr auto kHandling = std::chrono::minutes{5};
inline constexpr auto kTravel = std::chrono::minutes{20};
inline constexpr auto kInspectionLifetime = std::chrono::days{7};
inline constexpr auto kMinimumSearchAge = std::chrono::hours{1};
[[nodiscard]] Dimensions sizeLimit();
[[nodiscard]] StoragePolicy defaultStoragePolicy();
[[nodiscard]] PostageCalculator defaultCalculator();
[[nodiscard]] PostalBranch defaultBranch(const std::string& id, const std::string& prefix);
[[nodiscard]] SortingPlan sortingPlan(const std::string& source, const std::string& target);
[[nodiscard]] PostalRoute postalRoute(const std::string& id,
                                      const std::vector<std::string>& branchIds);
} // namespace post::cli
