function(learnwebgpu_setup_dependencies)
  set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)
  include(cmake/CPM.cmake)
  cpmusepackagelock(package-lock.cmake)

  message(STATUS "Include fmtlib")
  cpmaddpackage("gh:fmtlib/fmt#e3ddede6c4ee818825c4e5a6dfa1d384860c27d9"
  )# 11.1.1
  message(STATUS "Added fmtlib with CPM as ${CPM_LAST_PACKAGE_NAME}")

  message(STATUS "Include GLFW")
  cpmaddpackage("gh:glfw/glfw#7b6aead9fb88b3623e3b3725ebb42670cbe4c579") # 3.4

  message(STATUS "Include GLFW3 WebGPU")
  cpmaddpackage(
    "gh:eliemichel/glfw3webgpu#39a80205998c6cbf803e18e750d964c23a47ef39"
  )# v1.2.0

  message(STATUS "Include spdlog")
  # cpmaddpackage("gh:gabime/spdlog#8e5613379f5140fefb0b60412fbf1f5406e7c7f8" )#
  # v1.15.0
  cpmaddpackage("gh:gabime/spdlog#276ee5f5c0eb13626bd367b006ace5eae9526d8a"
  )# v1.x release fixes fmt compatibility issue

  cpmaddpackage(
    "gh:eliemichel/WebGPU-distribution#992fef64da25072ebe3844a73f7103105e7fd133"
  ) # wgpu-static-v0.19.4.1 + fix
endfunction()
