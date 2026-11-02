cmake_minimum_required(VERSION 3.20)
project(McOsuVrmInspect LANGUAGES CXX)
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
