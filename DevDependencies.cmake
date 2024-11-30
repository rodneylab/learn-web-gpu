include(${PROJECT_SOURCE_DIR}/cmake/CPM.cmake)

function(learnwebgpu_setup_dev_dependencies)
  set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

  message(STATUS "Include Catch2")
  cpmaddpackage("gh:catchorg/Catch2#fa43b77429ba76c462b1898d6cd2f2d7a9416b14"
  )# v3.7.1
endfunction()
