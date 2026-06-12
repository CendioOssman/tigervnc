if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "cmake must be invoked with the top level directory")
endif()

add_executable(vncviewer
  menukey.cxx
  AuthDialog.cxx
  CConn.cxx
  DesktopWindow.cxx
  EmulateMB.cxx
  ServerDialog.cxx
  OptionsCompression.cxx
  OptionsDialog.cxx
  OptionsDisplay.cxx
  OptionsInput.cxx
  OptionsMisc.cxx
  OptionsPage.cxx
  OptionsSecurity.cxx
  PlatformPixelBuffer.cxx
  QMonitorArrangement.cxx
  QStyles.cxx
  Toast.cxx
  Viewport.cxx
  clicksalternativegesture.cxx
  clicksalternativegesturerecognizer.cxx
  i18n.cxx
  mainloop.cxx
  panzoomgesture.cxx
  panzoomgesturerecognizer.cxx
  parameters.cxx
  MonitorIndicesParameter.cxx
  tapdraggesture.cxx
  tapdraggesturerecognizer.cxx
  tunnelfactory.cxx
  vncviewer.cxx)

if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  set_target_properties(vncviewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(WIN32)
  # Since vncviewer.rc is generated, local includes will be looking
  # in the wrong directory. We need to help it out.
  target_include_directories(vncviewer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  configure_file(vncviewer.rc.in vncviewer.rc)
  target_sources(vncviewer PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/vncviewer.rc)
endif()

if(WIN32)
  target_sources(vncviewer PRIVATE win32.c)
  target_sources(vncviewer PRIVATE KeyboardWin32.cxx keysym2ucs.c)
elseif(APPLE)
  target_sources(vncviewer PRIVATE cocoa.mm)
  target_sources(vncviewer PRIVATE KeyboardMacOS.mm osx_to_qnum.c keysym2ucs.c)
else()
  target_sources(vncviewer PRIVATE x11.cxx)
  target_sources(vncviewer PRIVATE KeyboardX11.cxx xkb_to_qnum.c)
endif()

target_sources(vncviewer PRIVATE res.qrc)

target_include_directories(vncviewer PUBLIC ${CMAKE_SOURCE_DIR}/common)
target_link_libraries(vncviewer PRIVATE rfb network rdr os)

target_include_directories(vncviewer SYSTEM PUBLIC ${GETTEXT_INCLUDE_DIR})
target_link_libraries(vncviewer PRIVATE ${GETTEXT_LIBRARIES})

set_target_properties(vncviewer PROPERTIES AUTOMOC ON)
target_link_libraries(vncviewer PRIVATE
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets)
if(UNIX AND NOT APPLE)
  if(QT_VERSION_MAJOR LESS_EQUAL 5)
    target_link_libraries(vncviewer PRIVATE
      Qt${QT_VERSION_MAJOR}::X11Extras)
  endif()
endif()

# FIXME: Not tested
if(BUILD_STATIC)
  if(QT_VERSION_MAJOR LESS_EQUAL 5)
    target_link_libraries(vncviewer PRIVATE
      Qt${QT_VERSION_MAJOR}::Gui
      Qt${QT_VERSION_MAJOR}::Network
      Qt${QT_VERSION_MAJOR}::Svg
      Qt${QT_VERSION_MAJOR}::ThemeSupport)
  endif()

  # Embed all translations that Qt needs
  get_target_property(QT${QT_VERSION_MAJOR}_QMAKE_EXECUTABLE
    Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
  execute_process(COMMAND
    ${QT${QT_VERSION_MAJOR}_QMAKE_EXECUTABLE}
      --query QT_INSTALL_TRANSLATIONS
    OUTPUT_VARIABLE QT_TRANSLATIONS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  file(TO_CMAKE_PATH "${QT_TRANSLATIONS_DIR}" QT_TRANSLATIONS_DIR)

  set(QT_TRANSLATIONS)
  file(GLOB TEMP_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qt_*.qm")
  list(APPEND QT_TRANSLATIONS ${TEMP_TRANSLATIONS})
  file(GLOB TEMP_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qtbase_*.qm")
  list(APPEND QT_TRANSLATIONS ${TEMP_TRANSLATIONS})
  file(GLOB TEMP_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qtscript_*.qm")
  list(APPEND QT_TRANSLATIONS ${TEMP_TRANSLATIONS})
  file(GLOB TEMP_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qtmultimedia_*.qm")
  list(APPEND QT_TRANSLATIONS ${TEMP_TRANSLATIONS})
  file(GLOB TEMP_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qtxmlpatterns_*.qm")
  list(APPEND QT_TRANSLATIONS ${TEMP_TRANSLATIONS})
  set(QT_TRANSLATIONS_CONFIGURE)
  foreach(QT_TR_PATH ${QT_TRANSLATIONS})
      get_filename_component(QT_TR_NAME ${QT_TR_PATH} NAME)
      if (NOT QT_TR_NAME MATCHES "(help)")
          list(APPEND QT_TRANSLATIONS_CONFIGURE
              "<file alias='${QT_TR_NAME}'>${QT_TR_PATH}</file>"
          )
      endif()
  endforeach()
  string(REPLACE ";" "\n" QT_TRANSLATIONS_CONFIGURE "${QT_TRANSLATIONS_CONFIGURE}")
  configure_file(qttranslations.qrc.in ${CMAKE_CURRENT_BINARY_DIR}/qttranslations.qrc @ONLY)
  target_sources(vncviewer PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/qttranslations.qrc)
endif()

if(WIN32)
  target_link_libraries(vncviewer PRIVATE msimg32 imm32)
elseif(APPLE)
  target_link_libraries(vncviewer PRIVATE "-framework Cocoa")
  target_link_libraries(vncviewer PRIVATE "-framework Carbon")
  target_link_libraries(vncviewer PRIVATE "-framework IOKit")
  target_link_libraries(vncviewer PRIVATE "-framework QuartzCore")
else()
  target_link_libraries(vncviewer PRIVATE ${X11_Xi_LIB})
  target_link_libraries(vncviewer PRIVATE ${X11_Xinerama_LIB})
  target_link_libraries(vncviewer PRIVATE ${X11_X11_LIB})
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(vncviewer)
endif()

install(TARGETS vncviewer DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
if(UNIX)
  install(FILES vncviewer.man DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man1 RENAME vncviewer.1)

  file(STRINGS ${CMAKE_SOURCE_DIR}/po/LINGUAS LINGUAS)
  set(po_FILES "")
  foreach(lang ${LINGUAS})
    list(APPEND po_FILES "${CMAKE_SOURCE_DIR}/po/${lang}.po")
  endforeach()

  configure_file(vncviewer.desktop.in.in vncviewer.desktop.in)
  find_program(INTLTOOL_MERGE_EXECUTABLE intltool-merge)
  if("${GETTEXT_VERSION_STRING}" VERSION_GREATER 0.18.99)
    add_custom_command(OUTPUT vncviewer.desktop
      COMMAND ${GETTEXT_MSGFMT_EXECUTABLE}
                --desktop --template vncviewer.desktop.in
                -d ${CMAKE_SOURCE_DIR}/po -o vncviewer.desktop
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/vncviewer.desktop.in
              ${po_FILES}
    )
  elseif(INTLTOOL_MERGE_EXECUTABLE)
    add_custom_command(OUTPUT vncviewer.desktop
      COMMAND sed -e 's/^Name/_Name/'
                -e 's/^GenericName/_GenericName/'
                -e 's/^Comment/_Comment/'
                vncviewer.desktop.in > vncviewer.desktop.intl
      COMMAND ${INTLTOOL_MERGE_EXECUTABLE}
                -d ${CMAKE_SOURCE_DIR}/po
                vncviewer.desktop.intl vncviewer.desktop
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/vncviewer.desktop.in
              ${po_FILES}
    )
  else()
    add_custom_command(OUTPUT vncviewer.desktop
      COMMAND cp vncviewer.desktop.in vncviewer.desktop
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/vncviewer.desktop.in
    )
  endif()
  add_custom_target(desktop ALL DEPENDS vncviewer.desktop)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vncviewer.desktop DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/applications)

  if("${GETTEXT_VERSION_STRING}" VERSION_GREATER 0.19.6)
    add_custom_command(OUTPUT org.tigervnc.vncviewer.metainfo.xml
      COMMAND ${GETTEXT_MSGFMT_EXECUTABLE}
                --xml --template ${CMAKE_CURRENT_SOURCE_DIR}/org.tigervnc.vncviewer.metainfo.xml.in
                -d ${CMAKE_SOURCE_DIR}/po -o org.tigervnc.vncviewer.metainfo.xml
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/org.tigervnc.vncviewer.metainfo.xml.in
              ${po_FILES}
    )
  elseif(INTLTOOL_MERGE_EXECUTABLE)
    add_custom_command(OUTPUT org.tigervnc.vncviewer.metainfo.xml
      COMMAND sed -e 's@<name>@<_name>@\;s@</name>@</_name>@'
                  -e 's@<summary>@<_summary>@\;s@</summary>@</_summary>@'
                  -e 's@<caption>@<_caption>@\;s@</caption>@</_caption>@'
                  -e 's@<p>@<_p>@g\;s@</p>@</_p>@g'
                ${CMAKE_CURRENT_SOURCE_DIR}/org.tigervnc.vncviewer.metainfo.xml.in > org.tigervnc.vncviewer.metainfo.xml.intl
      COMMAND ${INTLTOOL_MERGE_EXECUTABLE}
                -x ${CMAKE_SOURCE_DIR}/po
                org.tigervnc.vncviewer.metainfo.xml.intl org.tigervnc.vncviewer.metainfo.xml
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/org.tigervnc.vncviewer.metainfo.xml.in
              ${po_FILES}
    )
  else()
    add_custom_command(OUTPUT org.tigervnc.vncviewer.metainfo.xml
      COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/org.tigervnc.vncviewer.metainfo.xml.in org.tigervnc.vncviewer.metainfo.xml
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/org.tigervnc.vncviewer.metainfo.xml.in
    )
  endif()
  add_custom_target(appstream ALL DEPENDS org.tigervnc.vncviewer.metainfo.xml)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.tigervnc.vncviewer.metainfo.xml DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/metainfo)

  foreach(res 16 22 24 32 48 64 128)
    install(FILES ../media/icons/tigervnc_${res}.png DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/${res}x${res}/apps RENAME tigervnc.png)
  endforeach()
  install(FILES ../media/icons/tigervnc.svg DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps)
endif()

if(WIN32)
    set(DEPLOY_DIR ${CMAKE_CURRENT_BINARY_DIR}/deploy CACHE PATH "Package directory")
    if(CMAKE_BUILD_TYPE MATCHES Release)
      set(RELDEB --release)
    else()
      set(RELDEB --debug)
    endif()
    get_target_property(LOCAL_QMAKE_PATH Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(LOCAL_QTBIN_PATH ${LOCAL_QMAKE_PATH} DIRECTORY)
    add_custom_command(
        TARGET vncviewer
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${DEPLOY_DIR}/locale>")
    add_custom_command(
        TARGET vncviewer
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<SHELL_PATH:$<TARGET_FILE:vncviewer>> "$<SHELL_PATH:${DEPLOY_DIR}>")
    if(NOT BUILD_STATIC)
    #if(NOT BUILD_STATIC AND (QT_VERSION_MAJOR EQUAL 6 OR (QT_VERSION_MAJOR EQUAL 5 AND QT_VERSION_MINOR GREATER_EQUAL 15)))
        find_program(DEPLOYQT NAMES windeployqt HINTS "${LOCAL_QTBIN_PATH}")
        #add_custom_command(
        #    TARGET vncviewer
        #    POST_BUILD
        #    COMMAND ${DEPLOYQT} ${RELDEB} --dir ${DEPLOY_DIR} --no-translations $<TARGET_FILE:vncviewer>)
    endif()

    if(ENABLE_NLS)
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../po/LINGUAS po_FILES)
    foreach(lang ${po_FILES})
      set(mo "${CMAKE_CURRENT_BINARY_DIR}/../po/${lang}.mo")
      add_custom_command(
          TARGET vncviewer
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E make_directory $<SHELL_PATH:${DEPLOY_DIR}/locale/${lang}>)
      add_custom_command(
          TARGET vncviewer
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy $<SHELL_PATH:${mo}> $<SHELL_PATH:${DEPLOY_DIR}/locale/${lang}/LC_MESSAGES/tigervnc.mo>)
    endforeach(lang)
    endif()
endif()
if(APPLE AND CMAKE_BUILD_TYPE MATCHES Release AND NOT BUILD_STATIC)
    set(DEPLOY_DIR $<TARGET_FILE_DIR:vncviewer>/../../../vncviewer.app CACHE PATH "Package directory")
    set(LOCALE_DIR $<TARGET_FILE_DIR:vncviewer>/../Resources/locale CACHE PATH "Locale directory")
    get_target_property(LOCAL_QMAKE_PATH Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(LOCAL_QTBIN_PATH ${LOCAL_QMAKE_PATH} DIRECTORY)
    find_program(DEPLOYQT NAMES macdeployqt HINTS "${LOCAL_QTBIN_PATH}")
    # Remove interim files (if any) before executing macdeployqt, because they often cause fatal errors.
    add_custom_command(
        TARGET vncviewer
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}")
    add_custom_command(
        TARGET vncviewer
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LOCALE_DIR}")
    add_custom_command(
        TARGET vncviewer
        POST_BUILD
        WORKING_DIRECTORY "${DEPLOY_DIR}"
        COMMAND ${DEPLOYQT} "${DEPLOY_DIR}" "-dmg" "-always-overwrite" "-verbose=2" "-no-strip" "-codesign=-")

        file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../po/LINGUAS po_FILES)
        foreach(lang ${po_FILES})
          set(mo "${CMAKE_CURRENT_BINARY_DIR}/../po/${lang}.mo")
          add_custom_command(
              TARGET vncviewer
              POST_BUILD
              COMMAND ${CMAKE_COMMAND} -E make_directory ${LOCALE_DIR}/${lang})
          add_custom_command(
              TARGET vncviewer
              POST_BUILD
              COMMAND ${CMAKE_COMMAND} -E copy ${mo} ${LOCALE_DIR}/${lang}/LC_MESSAGES/tigervnc.mo)
        endforeach(lang)
endif()
if(UNIX)
    set(DEPLOY_DIR ${CMAKE_CURRENT_BINARY_DIR}/deploy CACHE PATH "Package directory")
    add_custom_command(
        TARGET vncviewer
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${DEPLOY_DIR}/locale>")

    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../po/LINGUAS po_FILES)
    foreach(lang ${po_FILES})
      set(mo "${CMAKE_CURRENT_BINARY_DIR}/../po/${lang}.mo")
      add_custom_command(
          TARGET vncviewer
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E make_directory $<SHELL_PATH:${DEPLOY_DIR}/locale/${lang}>)
      add_custom_command(
          TARGET vncviewer
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy $<SHELL_PATH:${mo}> $<SHELL_PATH:${DEPLOY_DIR}/locale/${lang}/LC_MESSAGES/tigervnc.mo>)
    endforeach(lang)
endif()
