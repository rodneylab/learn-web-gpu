function(learnwebgpu_setup_dependencies)
  set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)
  include(cmake/CPM.cmake)
  cpmusepackagelock(package-lock.cmake)

  message(STATUS "Include fmtlib")
  cpmaddpackage("gh:fmtlib/fmt#0c9fce2ffefecfdce794e1859584e25877b7b592"
  )# 11.0.2

  message(STATUS "Include GLFW")
  cpmaddpackage("gh:glfw/glfw#7b6aead9fb88b3623e3b3725ebb42670cbe4c579") # 3.4

  message(STATUS "Include GLFW3 WebGPU")
  cpmaddpackage(
    "gh:eliemichel/glfw3webgpu#39a80205998c6cbf803e18e750d964c23a47ef39"
  )# v1.2.0

  message(STATUS "Include spdlog")
  cpmaddpackage("gh:gabime/spdlog#8e5613379f5140fefb0b60412fbf1f5406e7c7f8"
  )# v1.15.0

  message(STATUS "Include WebGPU-distribution")
  cpmaddpackage(
    "gh:eliemichel/WebGPU-distribution#b763f1277733bf5ccc07202f7ab51ad1fdb64453"
  )
  # wgpu-v0.19.4.1
endfunction()
