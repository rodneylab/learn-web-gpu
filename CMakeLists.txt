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

cpmaddpackage("gh:glfw/glfw#e2c92645460f680fd272fd2eed591efb2be7dc31"
)# 3.3-stable
cpmaddpackage(
  "gh:eliemichel/glfw3webgpu#e4cd9131c7472b000641a104116c2fecf13d55a7") # v1.1.0
cpmaddpackage("gh:gabime/spdlog#7c02e204c92545f869e2f04edaab1f19fe8b19fd"
)# v1.13.0
if(EMSCRIPTEN OR WEBGPU_BACKEND_U STREQUAL "EMSCRIPTEN")
  cpmaddpackage(
    "gh:eliemichel/WebGPU-distribution#5ece5a9ad125cb243e3e9b009d03469d663a30d4"
  ) # emscripten
else()
  cpmaddpackage(
    "gh:eliemichel/WebGPU-distribution#9dd47f8515dfd7112b750da07e719460a88bf2e8"
  ) # wgpu-5433868 (works with Swap Chain)
  # cpmaddpackage(
  # "gh:eliemichel/WebGPU-distribution#def06a045c3fa1478e1b1047bb81b92f97b8483d"
  # ) # wgpu (Swap Chain removed)
endif()

add_library(WebGPUUtils_lib STATIC "${PROJECT_SOURCE_DIR}/src/webgpu-utils.cpp")
target_link_libraries(WebGPUUtils_lib spdlog webgpu learnwebgpu_compiler_flags)

add_executable(App src/main.cpp)
target_link_libraries(App PUBLIC WebGPUUtils_lib glfw spdlog webgpu glfw3webgpu
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
