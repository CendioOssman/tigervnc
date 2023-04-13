add_library(network STATIC
  Socket.cxx
  Socket.h
  TcpSocket.cxx
  TcpSocket.h
  )

if(NOT WIN32)
  target_sources(network PRIVATE UnixSocket.cxx UnixSocket.h)
endif()

target_include_directories(network PUBLIC ${CMAKE_SOURCE_DIR}/common)
target_link_libraries(network os rdr rfb)

if(UNIX)
  libtool_create_control_file(network)
endif()
