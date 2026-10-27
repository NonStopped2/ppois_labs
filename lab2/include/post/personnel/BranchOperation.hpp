#pragma once
namespace post {
enum class BranchOperation {
    StartShift,
    AcceptShipment,
    IssueShipment,
    Dispatch,
    ReceiveBatch,
    ApproveReturn,
    InspectVehicle,
    ManageShift,
    DeliverShipment,
    DriveVehicle
};
} // namespace post
