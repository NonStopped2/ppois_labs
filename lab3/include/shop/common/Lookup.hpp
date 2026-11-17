#pragma once

#include "shop/common/EntityNotFoundException.hpp"
#include <string>
#include <string_view>

namespace shop::detail {

template <class Error> void check(bool condition, std::string_view message) {
    if (!condition) {
        throw Error(std::string(message));
    }
}

template <class Map> const typename Map::mapped_type& get(const Map& values, std::string_view id) {
    const auto found = values.find(std::string(id));
    if (found == values.end()) {
        throw EntityNotFoundException("Не найден объект: " + std::string(id));
    }
    return found->second;
}

} // namespace shop::detail
