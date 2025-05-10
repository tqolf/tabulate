cmake_minimum_required(VERSION 3.0)
project("table maker for modern c++")

option(COV "Enable coverage" OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND COV)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 --coverage")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 --coverage")
endif()

enable_testing()

add_library(tabulate tabulate.cc)
target_sources(tabulate PUBLIC FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES tabulate.h)

file(GLOB files samples/*.cc)
add_custom_target(tabulate-all)
foreach (file ${files})
  get_filename_component(sample ${file} NAME_WE)
  add_executable(tabulate-${sample} ${file})
  target_link_libraries(tabulate-${sample} tabulate gtest gtest_main)
  target_include_directories(
    tabulate-${sample} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  )

  if (NOT EXISTS "${file}.skip")
    add_test(
      NAME tabulate-${sample}
      COMMAND tabulate-${sample}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    add_dependencies(tabulate-all tabulate-${sample})
  endif ()
endforeach ()
