cmake_minimum_required(VERSION 3.20)
project(McOsuSpringTests LANGUAGES CXX)
enable_testing()
get_filename_component(MCOSU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(MCENGINE_ROOT "${MCOSU_ROOT}/third_party/McEngine/McEngine" CACHE PATH "McEngine source directory")
add_library(spring-core STATIC
    "${MCOSU_ROOT}/src/App/Osu/Avatar/Spring/SpringDefinition.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/Spring/SpringMath.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/Spring/SpringRuntime.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmDocument.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/VrmTransforms.cpp"
    "${MCOSU_ROOT}/src/App/Osu/Avatar/PoseMath.cpp"
    "${MCENGINE_ROOT}/src/Util/Matrices.cpp")
target_compile_features(spring-core PUBLIC cxx_std_11)
target_include_directories(spring-core PUBLIC "${MCOSU_ROOT}/src/App/Osu/Avatar"
    "${MCENGINE_ROOT}/src/Util" "${MCOSU_ROOT}/third_party/cgltf" "${MCOSU_ROOT}/third_party/nlohmann")
add_executable(spring-math-tests math-tests.cpp)
target_link_libraries(spring-math-tests PRIVATE spring-core)
add_test(NAME spring-math COMMAND spring-math-tests)
add_executable(spring-runtime-tests runtime-tests.cpp)
target_link_libraries(spring-runtime-tests PRIVATE spring-core)
add_test(NAME spring-runtime COMMAND spring-runtime-tests "${CMAKE_CURRENT_BINARY_DIR}/synthetic-spring.vrm")
add_executable(spring-runtime-audit runtime-audit.cpp)
target_link_libraries(spring-runtime-audit PRIVATE spring-core)
