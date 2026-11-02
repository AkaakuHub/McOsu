cmake_minimum_required(VERSION 3.20)
project(McOsuTrackingInspect LANGUAGES CXX)
get_filename_component(MCOSU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(MCENGINE_ROOT "${MCOSU_ROOT}/third_party/McEngine/McEngine" CACHE PATH "McEngine source directory")
add_executable(vr-tracking-inspect main.cpp)
target_compile_features(vr-tracking-inspect PRIVATE cxx_std_11)
target_include_directories(vr-tracking-inspect PRIVATE
    "${MCOSU_ROOT}/third_party/openvr-input" "${MCOSU_ROOT}/third_party/nlohmann")
target_link_libraries(vr-tracking-inspect PRIVATE "${MCENGINE_ROOT}/libraries/openvr/lib/windows/openvr_api.lib")
