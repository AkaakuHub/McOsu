cmake_minimum_required(VERSION 3.20)
project(McOsuVrmInspect LANGUAGES CXX)
enable_testing()
get_filename_component(MCOSU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_executable(vrm-inspect main.cpp "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmDocument.cpp")
target_sources(vrm-inspect PRIVATE "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmGeometry.cpp")
target_sources(vrm-inspect PRIVATE "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmSkinning.cpp")
target_compile_features(vrm-inspect PRIVATE cxx_std_11)
target_include_directories(vrm-inspect PRIVATE
    "${MCOSU_ROOT}/third_party/McEngine/McEngine/src/Util"
    "${MCOSU_ROOT}/src/App/Osu/Avatar"
    "${MCOSU_ROOT}/third_party/cgltf"
    "${MCOSU_ROOT}/third_party/nlohmann")
add_executable(pose-math-test pose-math-test.cpp "${MCOSU_ROOT}/src/App/Osu/Avatar/PoseMath.cpp"
    "${MCOSU_ROOT}/third_party/McEngine/McEngine/src/Util/Matrices.cpp")
target_compile_features(pose-math-test PRIVATE cxx_std_11)
target_include_directories(pose-math-test PRIVATE
    "${MCOSU_ROOT}/src/App/Osu/Avatar"
    "${MCOSU_ROOT}/third_party/McEngine/McEngine/src/Util")
add_test(NAME pose-math COMMAND pose-math-test)
add_executable(vrm-pose-audit pose-audit.cpp
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmDocument.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmGeometry.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmSkinning.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmPose.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmPoseArm.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmHandAnchors.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmTransforms.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/PoseMath.cpp"
    "${MCOSU_ROOT}/third_party/McEngine/McEngine/src/Util/Matrices.cpp")
target_compile_features(vrm-pose-audit PRIVATE cxx_std_11)
target_include_directories(vrm-pose-audit PRIVATE
    "${MCOSU_ROOT}/src/App/Osu/Avatar"
    "${MCOSU_ROOT}/third_party/cgltf"
    "${MCOSU_ROOT}/third_party/nlohmann"
    "${MCOSU_ROOT}/third_party/McEngine/McEngine/src/Util")
