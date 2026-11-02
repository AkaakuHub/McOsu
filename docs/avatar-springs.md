# VRM SpringBone runtime

`avatar::SpringRuntime` evaluates the standard `VRMC_springBone` 1.0 extension on the CPU. It reads the declared joint pairs, sphere/capsule colliders, collider groups and per-chain centers. A chain's final joint is a tail only; it does not create a synthetic end joint.

Construct it with `VrmDocument&` before animation modifies the rest skeleton. Each frame, apply the body pose, then call `update(deltaTime, modelToWorld)` once before skinning. Pass the same outer model transform that rendering uses so translation and yaw outside the glTF hierarchy participate in world-space inertia. Rendering multiple eyes must reuse that result instead of advancing physics for each eye.

`reset()` discards tail history. A zero delta time reapplies the existing tail pose without advancing inertia. A delta time greater than 0.1 seconds discards history and applies the current animation pose, avoiding a large impulse after suspension. Negative or non-finite delta times are rejected.

The reference Verlet integration and sequential collider pushout follow the [VRM SpringBone specification](https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_springBone-1.0/README.md) and [UniVRM 0.131.4](https://github.com/vrm-c/UniVRM/tree/v0.131.4/Packages/UniGLTF/Runtime/SpringBoneJobs). Collider radii use the maximum world-axis scale. The joint length is restored after each pushout. This sequential projection can leave residual penetration when collider constraints conflict; it is not continuous cloth collision or a guarantee of non-intersection.

`tools/spring-tests` contains the math tests and an external-model runtime audit. Configure it with CMake and provide `MCENGINE_ROOT` when building outside a populated engine submodule. Run `ctest`, then `spring-runtime-audit path/to/avatar.vrm`. The audit moves the outer model transform, checks finite poses and joint lengths, and verifies history reset after suspension. No avatar data is included in the repository.

Synthetic runtime tests generate and remove a minimal GLB in the build directory. They distinguish center-relative movement from world-space inertia, verify that 0.25/1/3 uniform scales preserve local simulation with scaled colliders and joint radii, and check zero-time updates and suspension reset. The synthetic fixture writer refuses to overwrite a pre-existing file.
