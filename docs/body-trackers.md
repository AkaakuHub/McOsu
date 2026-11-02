# Body trackers

`OpenVRBodyTracking` reads generic-tracker render poses from the existing scene compositor's `GetLastPoses` cache after the engine calls `WaitGetPoses`. It requires standing tracking space and returns unavailable if the compositor, system, or cached poses cannot be read. It does not initialize a runtime, set an action manifest, poll controller buttons, or change tracking origins. `sample()` uses the same cached render-pose prediction as the engine's HMD and hands.

Automatic assignment accepts only exact `Prop_ControllerType_String` values `vive_tracker_waist`, `vive_tracker_left_foot`, and `vive_tracker_right_foot`. These names are documented by [HTC's native tracker integration guide](https://developer.vive.com/us/support/sdk/category_howto/how-do-i-use-wrist-tracker-as-tracker-unity.html). Other tracker types remain unassigned; their spatial position never determines their role.

`setBindings(waistSerial, leftFootSerial, rightFootSerial)` overrides individual roles by exact device serial. Empty strings enable automatic role assignment. Explicit serials reserve that device from automatic assignment to another role. Repeated serial bindings and multiple candidates invalidate the affected roles. `devices()` exposes the most recent generic-device enumeration, including serial, controller type, connection state, and raw sensor pose.

Each sample independently evaluates connections, role properties and pose validity. Only connected devices with `bPoseIsValid`, `TrackingResult_Running_OK`, and finite matrices produce valid output. Missing or invalid devices clear their own role immediately. Pose output is the sensor's raw transform; avatar-specific sensor-to-bone calibration belongs to the pose solver.

The cached compositor render poses follow [Valve's IVRCompositor interface](https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h). Independent waist/foot target availability follows the approach visible in [Custom Avatars AvatarIK](https://github.com/nicoco007/BeatSaberCustomAvatars/blob/main/Source/CustomAvatar/Avatar/AvatarIK.cs). No source code was copied from Custom Avatars.

Changes to the runtime's raw-to-standing transform produce `trackingSpaceDelta = newRawToStanding * inverse(previousRawToStanding)`. The caller applies this transform to stored standing-space contact points. The first sample, unavailable interfaces, changed interface pointers and invalid origin matrices reset this history. This describes changes of the calibrated standing origin; it cannot identify an independent discontinuity of raw tracking coordinates.

The standalone `tools/body-tracking-tests` target tests assignment, partial loss, device removal, role changes, ambiguity, and explicit serial reservations using synthetic devices. It does not claim hardware tracker verification.
