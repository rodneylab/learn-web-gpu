cmake_minimum_required(VERSION 3.15...3.28)
project(
  LearnWebGPU
  VERSION 0.1.0
  LANGUAGES CXX C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

add_library(learnwebgpu_compiler_flags INTERFACE)
target_compile_features(learnwebgpu_compiler_flags INTERFACE cxx_std_17)

set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")
target_compile_options(
  learnwebgpu_compiler_flags
  INTERFACE
    "$<${gcc_like_cxx}:$<BUILD_INTERFACE:-pedantic-errors;-Werror;-Wall;-Weffc++;-Wextra;-Wconversion;-Wsign-conversion>>"
    "$<${msvc_cxx}:$<BUILD_INTERFACE:-W4>>")

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${PROJECT_SOURCE_DIR}/cmake")
option(RUN_TESTS "Enable tests" ON)
include(coverage)
add_coverage_target("Catch_tests/*")

include(utils.cmake)
include(cmake/CPM.cmake)

message(STATUS "Include GLFW")
cpmaddpackage("gh:glfw/glfw#7b6aead9fb88b3623e3b3725ebb42670cbe4c579") # 3.4
message(STATUS "Include GLFW3 WebGPU")
cpmaddpackage(
  "gh:eliemichel/glfw3webgpu#39a80205998c6cbf803e18e750d964c23a47ef39") # v1.2.0
message(STATUS "Include spdlog")
cpmaddpackage("gh:gabime/spdlog#27cb4c76708608465c413f6d0e6b8d99a4d84302"
)# v1.14.1
message(STATUS "Include WebGPU-distribution")
cpmaddpackage(
  "gh:eliemichel/WebGPU-distribution#b763f1277733bf5ccc07202f7ab51ad1fdb64453")
# wgpu-v0.19.4.1

add_executable(App src/main.cpp)
target_link_libraries(App PUBLIC glfw spdlog webgpu glfw3webgpu
                                 learnwebgpu_compiler_flags)

if(XCODE)
  set_target_properties(
    App PROPERTIES XCODE_GENERATE_SCHEME ON
                   XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

target_copy_webgpu_binaries(App)

option(RUN_UNIT_TESTS "Run Catch2 unit tests" ON)
if(RUN_UNIT_TESTS)
  enable_testing()
  add_subdirectory(Catch_tests)
endif()
