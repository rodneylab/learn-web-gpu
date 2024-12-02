add_executable(App main.cpp)
target_include_directories(App PUBLIC .)
target_link_libraries(App PUBLIC fmt::fmt glfw spdlog webgpu glfw3webgpu
                                 learnwebgpu_compiler_flags)
target_compile_definitions(App PRIVATE SPDLOG_FMT_EXTERNAL)
set_target_properties(App PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")

if(XCODE)
  set_target_properties(
    App PROPERTIES XCODE_GENERATE_SCHEME ON
                   XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

target_copy_webgpu_binaries(App)
