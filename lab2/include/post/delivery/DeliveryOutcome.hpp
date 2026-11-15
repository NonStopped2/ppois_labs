#pragma once
namespace post {
enum class DeliveryOutcome { Delivered, Failed };
enum class DeliveryFailureReason { None, RecipientAbsent, AddressNotFound, Refused, Damaged };
} // namespace post
