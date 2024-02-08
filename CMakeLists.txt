cmake_minimum_required(VERSION 3.15...3.25)
project(
  LearnWebGPU
  VERSION 0.1.0
  LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
add_library(learnwebgpu_compiler_flags INTERFACE)
target_compile_features(learnwebgpu_compiler_flags INTERFACE cxx_std_17)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${PROJECT_SOURCE_DIR}/cmake")
option(RUN_TESTS "Enable tests" ON)
include(coverage)
add_coverage_target("Catch_tests/*")

include(utils.cmake)
include(cmake/CPM.cmake)

cpmaddpackage("gh:glfw/glfw#e2c92645460f680fd272fd2eed591efb2be7dc31"
)# 3.3-stable
cpmaddpackage("gh:fmtlib/fmt#e69e5f977d458f2650bb346dadf2ad30c5320281") # 10.x
cpmaddpackage(
  "gh:eliemichel/WebGPU-distribution#def06a045c3fa1478e1b1047bb81b92f97b8483d"
)# wgpu
cpmaddpackage(
  "gh:eliemichel/glfw3webgpu#e4cd9131c7472b000641a104116c2fecf13d55a7")# v1.1.0

add_executable(App src/main.cpp)
target_link_libraries(App PRIVATE fmt glfw webgpu glfw3webgpu learnwebgpu_compiler_flags)

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
