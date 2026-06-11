add_executable(vncconfig WIN32
  Legacy.cxx
  PasswordDialog.cxx
  vncconfig.cxx
  vncconfig.rc)

target_include_directories(vncconfig PUBLIC ${CMAKE_BINARY_DIR}/win)
target_link_libraries(vncconfig PRIVATE rfb_win32 rfb network rdr)
target_link_libraries(vncconfig PRIVATE ws2_32)

install(TARGETS vncconfig
  RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
)
