#pragma once
namespace post {
enum class DeliveryTaskState {
    Draft,
    Scheduled,
    InProgress,
    Failed,
    ReturnedToBranch,
    Delivered,
    Cancelled
};
} // namespace post
