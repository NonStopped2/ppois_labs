#pragma once

#include "shop/application/StoreState.hpp"
#include <filesystem>

namespace shop {
/** SnapshotStore.
 * Загружает и сохраняет файл снимка. Подменяет состояние только после успешного разбора.
 */
class SnapshotStore {
  public:
    static void save(const std::filesystem::path& destination, const StoreState& state);
    static void load(const std::filesystem::path& source, StoreState& state);
};
} // namespace shop
