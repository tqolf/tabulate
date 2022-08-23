CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT("Table Maker for Modern C++")

INCLUDE(cmake/ConfigureTarget.cmake)

INCLUDE(cmake/FindYamlCpp.cmake)
IF (YAMLCPP_FOUND)
  INCLUDE_DIRECTORIES(${YAMLCPP_INCLUDE_DIR})
  LINK_LIBRARIES(${YAMLCPP_LIBRARY})
ENDIF ()

INCLUDE(cmake/FindJsonCpp.cmake)
IF (JSONCPP_FOUND)
  INCLUDE_DIRECTORIES(${JSONCPP_INCLUDE_DIR})
  LINK_LIBRARIES(${JSONCPP_LIBRARY})
ENDIF ()

LINK_DIRECTORIES(/usr/local/lib/ /usr/local/opt/openssl@3/lib/)
INCLUDE_DIRECTORIES(/usr/local/include/ /usr/local/opt/openssl@3/include/)
LINK_LIBRARIES(roaring crypto)

ENABLE_TESTING()
FOREACH (category tabulate cxxopt progressbar threadpool misc)
  FILE(GLOB files samples/${category}/*.cc)
  ADD_CUSTOM_TARGET(${category})
  FOREACH (file ${files})
    GET_FILENAME_COMPONENT(sample ${file} NAME_WE)
    ADD_EXECUTABLE(${category}-${sample} ${file})
    TARGET_INCLUDE_DIRECTORIES(
      ${category}-${sample} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )
    ADD_TEST(NAME ${category}-${sample} COMMAND ${category}-${sample}
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    CONFIGURETARGET(${category}-${sample})
    ADD_DEPENDENCIES(${category} ${category}-${sample})
  ENDFOREACH ()
ENDFOREACH ()
