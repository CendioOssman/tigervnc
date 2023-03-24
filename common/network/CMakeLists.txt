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
target_compile_definitions(network
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

if(WIN32)
        target_link_libraries(network Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Widgets ws2_32)
endif()

if(UNIX)
  libtool_create_control_file(network)
endif()
