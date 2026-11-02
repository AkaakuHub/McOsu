# Avatar foot locomotion

`avatar::FootLocomotion` calculates foot targets independently from head/hand tracking and the two-bone leg solver. `configure()` receives model-space rest foot transforms, pelvis position, and each leg's total length. `update()` receives tracking-space metres, a model-to-tracking transform, current hips, a body balance estimate, and optional measured feet. The returned foot transforms have unit scale.

Planted targets remain fixed in tracking space. A one-second, duration-weighted history filters only the movement decision: signed linear/angular velocity selects between historical and current body position/heading, with a one-second weight decay. Translation, heading error, distance beyond the foot support segment, and leg reach select one step at a time. Step destinations remain fixed throughout each step. Shorter steps produce lower arcs. The existing scene has a horizontal floor at Y=0; there is no terrain query or collision detection in this module.

A valid foot tracker supplies its position and rotation without filtering or floor projection. The other foot can still use estimation. Both measured feet with a measured pelvis disable automatic stepping. On tracker loss, the last measured foot pose becomes the step start, including its measured rotation and elevation, rather than snapping to the rest pose. A rigid `trackingSpaceDelta` maps old tracking coordinates into new coordinates; it transforms planted feet, active step endpoints, floor elevation, and the complete motion history together. This supports yaw/translation changes on a horizontal floor, not tilted terrain.

## References and implementation boundary

Reviewed on 2026-11-03:

- [Beat Saber Custom Avatars: CustomIKSolverVR.cs](https://github.com/nicoco007/BeatSaberCustomAvatars/blob/main/Source/CustomAvatar/Avatar/CustomIKSolverVR.cs), `CustomLocomotion.CalculateAveragePose` and `AdjustStepOffsets`: movement-specific historical smoothing and short-step height policy.
- [Beat Saber Custom Avatars: AvatarIK.cs](https://github.com/nicoco007/BeatSaberCustomAvatars/blob/main/Source/CustomAvatar/Avatar/AvatarIK.cs), `UpdateSolverTargets`, `UpdateLocomotion`, `ApplyPlatformMotion`: per-device measurement priority, full-tracking locomotion disablement, and coordinate-space motion propagation.
- AvatarLab's `EstimatedBodyPose` and `EstimatedFootPlacement`: model-scaled stance, one moving foot, fixed step endpoints, and a smooth step trajectory.

This is an independent C++ implementation of those policies and mathematical operations. It does not contain Final IK code, Harmony patches, or copied Beat Saber source. Beat Saber Custom Avatars is LGPL-3.0-or-later; its referenced files credit Nicolas Gnyra and contributors, 2018–2026. Final IK's private solver and VRChat's solver are not reproduced here. Three-point inputs do not measure real knees or feet, so inferred targets cannot reproduce unobserved leg movement.

## Verification

The standalone `tools/locomotion-tests` CMake project checks stationary sensor jitter, head lean, small weight shifts, crouching, translation, turning, the single-step invariant, 30/60/90Hz results, exact measured-foot priority, full tracking, tracker loss, and coordinate rebasing while planted and while stepping. `MCENGINE_ROOT` selects the McEngine source directory. These are synthetic core tests; they do not establish visual quality on a headset.
