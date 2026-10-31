#pragma once
#include "post/branches/PostalBranch.hpp"
#include "post/sorting/SortingPlan.hpp"
#include "post/tariffs/PostageCalculator.hpp"
#include "post/transport/PostalRoute.hpp"
#include <array>
#include <chrono>
#include <utility>

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
inline constexpr auto kQuoteLifetime = std::chrono::hours{24};
inline constexpr auto kMaxRouteDuration = std::chrono::hours{24};
inline constexpr std::int64_t kMaxSideMm = 1000;
inline constexpr std::int64_t kCellWeight = 100000;
inline constexpr std::int64_t kCellCapacity = 100;
inline constexpr std::int64_t kFreeStorageDays = 2;
inline constexpr std::int64_t kMaxStorageDays = 7;
inline constexpr std::int64_t kDailyStorageFee = 50;
inline constexpr std::int64_t kMaxDeliveryAttempts = 3;
inline constexpr std::int64_t kMinRetryHours = 1;
inline constexpr std::int64_t kDomesticBasisPoints = 10000;
inline constexpr std::int64_t kStandardBase = 300, kExpressBase = 600;
inline constexpr std::int64_t kWeightStepGrams = 1000;
inline constexpr std::int64_t kStandardStep = 75, kExpressStep = 100;
inline constexpr std::int64_t kRegistrationFee = 100, kCourierFee = 250;
inline constexpr std::int64_t kFragileFee = 100, kNotificationFee = 50;
inline constexpr std::int64_t kDeclaredValueBasisPoints = 100;
inline constexpr std::array kBranches{std::pair{"A", "22"}, std::pair{"B", "23"},
                                      std::pair{"C", "24"}};
[[nodiscard]] Dimensions sizeLimit();
[[nodiscard]] StoragePolicy defaultStoragePolicy();
[[nodiscard]] PostageCalculator defaultCalculator();
[[nodiscard]] PostalBranch defaultBranch(const std::string& id, const std::string& prefix);
[[nodiscard]] SortingPlan sortingPlan(const std::string& source, const std::string& target);
[[nodiscard]] PostalRoute postalRoute(const std::string& id,
                                      const std::vector<std::string>& branchIds);
} // namespace post::cli
