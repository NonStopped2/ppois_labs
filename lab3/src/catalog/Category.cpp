#include "shop/catalog/Category.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

Category::Category(std::string_view id, std::string_view name, std::optional<std::string> parentId)
    : id_(detail::text(id, "ID категории")), name_(detail::text(name, "категория")),
      parentId_(std::move(parentId)) {
    if (parentId_) {
        *parentId_ = detail::text(*parentId_, "родительская категория");
        detail::require(*parentId_ != id_, "Категория не может быть своим родителем");
    }
}
const std::string& Category::id() const noexcept {
    return id_;
}
const std::string& Category::name() const noexcept {
    return name_;
}
const std::optional<std::string>& Category::parentId() const noexcept {
    return parentId_;
}
bool Category::archived() const noexcept {
    return archived_;
}

} // namespace shop
