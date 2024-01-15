cmake_minimum_required(VERSION 3.15...3.25)
project(
  LearnWebGPU
  VERSION 0.1.0
  LANGUAGES CXX C)

include(utils.cmake)
include(cmake/CPM.cmake)

cpmaddpackage("gh:glfw/glfw#e2c92645460f680fd272fd2eed591efb2be7dc31"
)# 3.3-stable
cpmaddpackage("gh:fmtlib/fmt#e69e5f977d458f2650bb346dadf2ad30c5320281") # 10.x
cpmaddpackage(
  "gh:eliemichel/WebGPU-distribution#def06a045c3fa1478e1b1047bb81b92f97b8483d"
)# wgpu

set(CMAKE_EXPORT_COMPILE_COMMANDS True)

add_executable(App src/main.cpp)
target_link_libraries(App PRIVATE fmt glfw webgpu)

set_target_properties(
  App
  PROPERTIES CXX_STANDARD 17
             CXX_EXTENSIONS OFF
             COMPILE_WARNING_AS_ERROR ON)

target_treat_all_warnings_as_errors(App)

if(XCODE)
  set_target_properties(
    App PROPERTIES XCODE_GENERATE_SCHEME ON
                   XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

target_copy_webgpu_binaries(App)
