add_library(rdr STATIC
  AESInStream.cxx
  AESInStream.h
  AESOutStream.cxx
  AESOutStream.h
  BufferedInStream.cxx
  BufferedInStream.h
  BufferedOutStream.cxx
  BufferedOutStream.h
  Exception.cxx
  Exception.h
  FdInStream.cxx
  FdInStream.h
  FdOutStream.cxx
  FdOutStream.h
  FileInStream.cxx
  FileInStream.h
  HexInStream.cxx
  HexInStream.h
  HexOutStream.cxx
  HexOutStream.h
  InStream.h
  InStream.cxx
  MemInStream.h
  MemOutStream.h
  OutStream.h
  RandomStream.cxx
  RandomStream.h
  TLSException.cxx
  TLSException.h
  TLSInStream.cxx
  TLSInStream.h
  TLSOutStream.cxx
  TLSOutStream.h
  ZlibInStream.cxx
  ZlibInStream.h
  ZlibOutStream.cxx
  ZlibOutStream.h
  types.h
  )

target_include_directories(rdr PUBLIC ${CMAKE_SOURCE_DIR}/common)
target_include_directories(rdr SYSTEM PUBLIC ${ZLIB_INCLUDE_DIRS})
target_link_libraries(rdr Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Widgets ${ZLIB_LIBRARIES} os rfb)

if(MSVC)
  # undef min and max macro
  target_compile_definitions(rfb PRIVATE NOMINMAX)
endif()
target_compile_definitions(rdr
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

if(GNUTLS_FOUND)
  target_include_directories(rdr SYSTEM PUBLIC ${GNUTLS_INCLUDE_DIR})
  target_link_libraries(rdr ${GNUTLS_LIBRARIES})
endif()
if (NETTLE_FOUND)
  target_include_directories(rdr SYSTEM PUBLIC ${NETTLE_INCLUDE_DIRS})
  target_link_libraries(rdr ${NETTLE_LIBRARIES})
  target_link_directories(rdr PUBLIC ${NETTLE_LIBRARY_DIRS})
endif()
if(WIN32)
	target_link_libraries(rdr ws2_32)
endif()

if(UNIX)
  libtool_create_control_file(rdr)
endif()
