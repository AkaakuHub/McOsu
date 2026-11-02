cmake_minimum_required(VERSION 3.20)
project(McOsuLocomotionTests LANGUAGES CXX)
enable_testing()
get_filename_component(MCOSU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(MCENGINE_ROOT "${MCOSU_ROOT}/third_party/McEngine/McEngine" CACHE PATH "McEngine source directory")
add_executable(locomotion-tests locomotion-tests.cpp
    "${MCOSU_ROOT}/src/App/Osu/Avatar/Locomotion/FootLocomotion.cpp"
    "${MCENGINE_ROOT}/src/Util/Matrices.cpp")
target_compile_features(locomotion-tests PRIVATE cxx_std_11)
target_include_directories(locomotion-tests PRIVATE "${MCOSU_ROOT}/src/App/Osu/Avatar"
    "${MCENGINE_ROOT}/src/Util")
add_test(NAME locomotion COMMAND locomotion-tests)
