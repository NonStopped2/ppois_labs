#pragma once

#include "shop/application/StoreState.hpp"
#include <filesystem>

namespace shop {
/** Explicit files only: no autosave, database, or crash-durability claim. */
class SnapshotStore {
  public:
    static void save(const std::filesystem::path& destination, const StoreState& state);
    static void load(const std::filesystem::path& source, StoreState& state);
};
} // namespace shop
