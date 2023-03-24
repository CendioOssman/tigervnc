add_library(os STATIC
  Mutex.cxx
  Thread.cxx
  os.cxx)

target_include_directories(os PUBLIC ${CMAKE_SOURCE_DIR}/common)
target_link_libraries(os rdr)
target_compile_definitions(os
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

if(UNIX)
  target_link_libraries(os Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Widgets pthread)
endif()
if(WIN32)
  target_link_libraries(os Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Widgets)
endif()

if(UNIX)
  libtool_create_control_file(os)
endif()
