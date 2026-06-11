include_directories(${CMAKE_SOURCE_DIR}/common)
include_directories(${CMAKE_SOURCE_DIR}/vncviewer)

add_executable(conv conv.cxx)
target_link_libraries(conv PRIVATE rfb)

add_executable(convertlf convertlf.cxx)
target_link_libraries(convertlf PRIVATE rfb)

add_executable(gesturehandler gesturehandler.cxx ../../vncviewer/GestureHandler.cxx)
target_link_libraries(gesturehandler PRIVATE rfb)

add_executable(hostport hostport.cxx)
target_link_libraries(hostport PRIVATE rfb)

add_executable(pixelformat pixelformat.cxx)
target_link_libraries(pixelformat PRIVATE rfb)

add_executable(unicode unicode.cxx)
target_link_libraries(unicode PRIVATE rfb)

add_executable(emulatemb emulatemb.cxx ../../vncviewer/EmulateMB.cxx)
target_link_libraries(emulatemb PRIVATE rfb)
target_include_directories(emulatemb SYSTEM PUBLIC ${GETTEXT_INCLUDE_DIR})
target_link_libraries(emulatemb PRIVATE ${GETTEXT_LIBRARIES})
