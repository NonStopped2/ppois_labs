#pragma once
#include "post/sorting/BatchStatus.hpp"
#include "post/sorting/MailBag.hpp"
#include "post/transport/PostalRoute.hpp"

namespace post {
// DispatchBatch — партия мешков между двумя отделениями для перевозки в TransportTrip.
// Хранит собственные мешки; после подготовки состав не меняется и соответствует маршруту.
// Проходит Draft → Ready → InTransit → Received; физическое выбытие проводит PostalBranch.
class DispatchBatch {
public:
    DispatchBatch(const std::string& id, const std::string& sourceBranchId,
                  const std::string& targetBranchId, std::chrono::sys_seconds createdAt);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& sourceBranchId() const noexcept;
    [[nodiscard]] const std::string& targetBranchId() const noexcept;
    [[nodiscard]] const std::vector<MailBag>& bags() const noexcept;
    [[nodiscard]] BatchStatus status() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds createdAt() const noexcept;
    void addBag(const MailBag& bag);
    void prepareForDispatch(const PostalRoute& route);
    void markReceived(const PostalBranch& branch, const ShipmentRegistry& registry,
                      std::chrono::sys_seconds time);
    bool operator==(const DispatchBatch& other) const = default;

private:
    friend class PostalBranch;
    void markDispatched();
    std::string id_;
    std::string sourceBranchId_;
    std::string targetBranchId_;
    std::vector<MailBag> bags_;
    BatchStatus status_{BatchStatus::Draft};
    std::chrono::sys_seconds createdAt_;
};
} // namespace post
