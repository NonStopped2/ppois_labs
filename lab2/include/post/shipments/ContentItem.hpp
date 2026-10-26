#pragma once

#include "post/core/Money.hpp"
#include "post/shipments/ContentCategory.hpp"

#include <cstdint>
#include <string>

namespace post {

class ContentItem {
public:
    ContentItem(const std::string& name, std::int64_t quantity, std::int64_t unitWeightGrams,
                Money unitDeclaredValue, ContentCategory category);

    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] std::int64_t quantity() const noexcept;
    [[nodiscard]] std::int64_t unitWeightGrams() const noexcept;
    [[nodiscard]] const Money& unitDeclaredValue() const noexcept;
    [[nodiscard]] ContentCategory category() const noexcept;

    [[nodiscard]] std::int64_t totalWeightGrams() const;
    [[nodiscard]] Money totalDeclaredValue() const;

    bool operator==(const ContentItem& other) const = default;

private:
    std::string name_;
    std::int64_t quantity_;
    std::int64_t unitWeightGrams_;
    Money unitDeclaredValue_;
    ContentCategory category_;
};

} // namespace post
