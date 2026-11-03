#include "OpenVRBodyTracking.h"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace avatar;
void require(bool value, const char *name)
{
    if (!value) { std::cerr << name << '\n'; std::exit(1); }
}
BodyTrackerDevice device(const char *serial, const char *type)
{
    BodyTrackerDevice value;
    value.serial = serial; value.controllerType = type; value.connected = true; value.pose.valid = true;
    return value;
}
int main()
{
    Matrix4 matrix;
    require(bodyTrackerPoseUsable(true, true, true, matrix), "normal tracking accepted");
    require(!bodyTrackerPoseUsable(false, true, true, matrix), "disconnected pose rejected");
    require(!bodyTrackerPoseUsable(true, false, true, matrix), "invalid native pose rejected");
    require(!bodyTrackerPoseUsable(true, true, false, matrix), "out of range tracking rejected");
    Matrix4 malformed(std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    require(!bodyTrackerPoseUsable(true, true, true, malformed), "nonfinite sensor matrix rejected");
    const std::array<std::string, 5> automatic = {{"", "", ""}};
    std::vector<BodyTrackerDevice> devices = {device("w", "vive_tracker_waist"), device("l", "vive_tracker_left_foot"), device("r", "vive_tracker_right_foot")};
    auto frame = assignBodyTrackers(devices, automatic);
    require(frame.waist.valid && frame.leftFoot.valid && frame.rightFoot.valid, "three explicit roles");
    devices[1].pose.valid = false;
    frame = assignBodyTrackers(devices, automatic);
    require(!frame.leftFoot.valid && frame.rightFoot.valid && frame.status[1] == BodyTrackingStatus::InvalidPose, "partial loss clears only affected role");
    devices.erase(devices.begin());
    require(!assignBodyTrackers(devices, automatic).waist.valid, "hot removal clears pose");
    devices = {device("x", "vive_tracker")};
    require(!assignBodyTrackers(devices, automatic).waist.valid, "unlabelled tracker is never inferred");
    const std::array<std::string, 5> explicitWaist = {{"x", "", ""}};
    require(assignBodyTrackers(devices, explicitWaist).waist.valid, "explicit serial assigns unlabelled tracker");
    const std::array<std::string, 5> duplicate = {{"x", "x", ""}};
    frame = assignBodyTrackers(devices, duplicate);
    require(!frame.waist.valid && !frame.leftFoot.valid && frame.status[0] == BodyTrackingStatus::Ambiguous, "duplicate serial binding rejected");
    devices.push_back(device("y", "vive_tracker_waist"));
    devices.push_back(device("z", "vive_tracker_waist"));
    require(assignBodyTrackers(devices, automatic).status[0] == BodyTrackingStatus::Ambiguous, "multiple role candidates rejected");
    devices = {device("x", "vive_tracker_waist")};
    const std::array<std::string, 5> overrideRole = {{"", "x", ""}};
    frame = assignBodyTrackers(devices, overrideRole);
    require(!frame.waist.valid && frame.leftFoot.valid, "explicit binding reserves tracker from auto role");
    devices[0].connected = false;
    require(!assignBodyTrackers(devices, overrideRole).leftFoot.valid, "disconnect clears explicit binding");
    devices[0].connected = true;
    devices[0].controllerType = "vive_tracker_right_foot";
    frame = assignBodyTrackers(devices, automatic);
    require(frame.rightFoot.valid && !frame.waist.valid, "role changes are reevaluated");
    devices = {device("w", "vive_tracker_waist"), device("l", "vive_tracker_left_foot"),
        device("r", "vive_tracker_right_foot"), device("le", "vive_tracker_left_elbow"), device("re", "vive_tracker_right_elbow")};
    frame = assignBodyTrackers(devices, automatic);
    require(frame.waist.valid && frame.leftFoot.valid && frame.rightFoot.valid && frame.leftElbow.valid && frame.rightElbow.valid, "all five body roles are tracked");
    devices[3].pose.valid = false;
    frame = assignBodyTrackers(devices, automatic);
    require(!frame.leftElbow.valid && frame.rightElbow.valid && frame.waist.valid, "elbow tracking loss is isolated");
    const std::array<std::string, 5> explicitFive = {{"w", "l", "r", "le", "re"}};
    devices[3].pose.valid = true;
    for (auto &tracker : devices) tracker.controllerType = "vive_ultimate_tracker";
    frame = assignBodyTrackers(devices, explicitFive);
    require(frame.waist.valid && frame.leftFoot.valid && frame.rightFoot.valid && frame.leftElbow.valid && frame.rightElbow.valid, "five Ultimate trackers bind by serial");
    std::cout << "Body tracker assignment checks passed\n";
}
