if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "cmake must be invoked with the top level directory")
endif()

add_library(test_util STATIC util.cxx)

add_executable(convperf convperf.cxx)
target_link_libraries(convperf PRIVATE test_util rfb)

add_executable(decperf decperf.cxx)
target_link_libraries(decperf PRIVATE test_util rdr rfb rfbclient)

add_executable(encperf encperf.cxx)
target_link_libraries(encperf PRIVATE test_util core rdr rfb rfbclient rfbserver)
