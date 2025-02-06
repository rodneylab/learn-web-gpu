add_executable(App main.cpp utilities/resource_manager.h)
target_link_libraries(App PRIVATE fmt spdlog::spdlog_header_only glfw webgpu
                                  glfw3webgpu learnwebgpu_compiler_flags)
set_target_properties(App PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
if(DEV_MODE)
  target_compile_definitions(
    App PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../resources"
                SPDLOG_FMT_EXTERNAL)
else()
  target_compile_definitions(App PRIVATE RESOURCE_DIR="./resources"
                                         SPDLOG_FMT_EXTERNAL)
endif()

target_copy_webgpu_binaries(App)

if(XCODE)
  set_target_properties(
    App PROPERTIES XCODE_GENERATE_SCHEME ON
                   XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

if(EMSCRIPTEN)
  set_target_properties(App PROPERTIES SUFFIX ".html")
  target_link_options(App PRIVATE -sASYNCIFY --preload-file
                      "${CMAKE_CURRENT_SOURCE_DIR}/../resources")
endif()
