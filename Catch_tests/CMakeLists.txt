include(${PROJECT_SOURCE_DIR}/DevDependencies.cmake)
learnwebgpu_setup_dev_dependencies()

add_executable(Catch_tests_run test.cpp)

target_link_libraries(Catch_tests_run PRIVATE learnwebgpu_compiler_flags)
target_link_libraries(Catch_tests_run PRIVATE Catch2::Catch2WithMain)
target_include_directories(Catch_tests_run PUBLIC "${PROJECT_SOURCE_DIR}/src")

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(Catch_tests_run)
