cmake_minimum_required(VERSION 3.20)
project(FullBodyTests LANGUAGES CXX)
get_filename_component(MCOSU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(AVATAR "${MCOSU_ROOT}/src/App/Osu/Avatar")
add_executable(full-body-tests main.cpp
    "${AVATAR}/VrmGeometry.cpp" "${AVATAR}/VrmSkinning.cpp"
    "${AVATAR}/VrmDocument.cpp" "${AVATAR}/VrmPose.cpp" "${AVATAR}/VrmPoseArm.cpp"
    "${AVATAR}/VrmPoseBody.cpp" "${AVATAR}/VrmTransforms.cpp" "${AVATAR}/VrmHandAnchors.cpp"
    "${AVATAR}/PoseMath.cpp" "${AVATAR}/Locomotion/FootLocomotion.cpp"
    "${MCOSU_ROOT}/third_party/McEngine/McEngine/src/Util/Matrices.cpp")
target_compile_features(full-body-tests PRIVATE cxx_std_11)
target_include_directories(full-body-tests PRIVATE "${AVATAR}" "${MCOSU_ROOT}/third_party/cgltf"
    "${MCOSU_ROOT}/third_party/nlohmann" "${MCOSU_ROOT}/third_party/McEngine/McEngine/src/Util")
