#pragma once
namespace post {
enum class ReturnStatus { Draft, Submitted, Approved, Rejected, Started };
enum class ReturnInitiatorKind { Customer, Employee };
enum class ReturnReason { SenderRequest, RecipientRefused, StorageExpired };
} // namespace post
