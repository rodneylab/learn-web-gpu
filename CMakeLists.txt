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

include(cmake/StaticAnalysers.cmake)
enable_clang_tidy()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${PROJECT_SOURCE_DIR}/cmake")
option(RUN_TESTS "Enable tests" ON)
include(coverage)
add_coverage_target("Catch_tests/*")

include(utils.cmake)
include(Dependencies.cmake)
learnwebgpu_setup_dependencies()

option(DEV_MODE "Set up development helper settings" ON)
add_subdirectory(src)

option(RUN_UNIT_TESTS "Run Catch2 unit tests" ON)
if(RUN_UNIT_TESTS)
  enable_testing()
  add_subdirectory(Catch_tests)
endif()
