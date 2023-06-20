if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "cmake must be invoked with the top level directory")
endif()

include_directories(${GETTEXT_INCLUDE_DIR})

include_directories(${CMAKE_SOURCE_DIR}/common)

#list(APPEND CMAKE_PREFIX_PATH "/opt/Qt/Static/6.2.4/gcc_64s")
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Quick Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Quick Widgets REQUIRED)
if(QT_VERSION_MAJOR LESS_EQUAL 5 AND BUILD_STATIC)
  find_package(QT NAMES Qt6 Qt5 COMPONENTS
               Gui
               Qml
               QmlWorkerScript
               QuickControls2
               Svg
               ThemeSupport
               REQUIRED)
  find_package(Qt5 COMPONENTS
               Gui
               Qml
               QmlWorkerScript
               QuickControls2
               Svg
               ThemeSupport
               REQUIRED)
endif()

if (WIN32)
  set(OS_DEP_SOURCES
      vncwinview.cxx
      vncwinview.h
      Surface_Win32.cxx
      win32.c
      win32.h
  )
elseif(APPLE)
  set(OS_DEP_SOURCES
      vncmacview.cxx
      vncmacview.h
      Surface_OSX.cxx
      cocoa.h
      cocoa.mm
      osx_to_qnum.c
  )
else()
  set(OS_DEP_SOURCES
      vncx11view.cxx
      vncx11view.h
      Surface_X11.cxx
      xkb_to_qnum.c
  )
endif()

set(PROJECT_SOURCES
        vncviewer.cxx
        vnctranslator.h
        appmanager.h
        appmanager.cxx
        vncapplication.cxx
        vncapplication.h
        vnctoast.cxx
        vnctoast.h
        vncwindow.cxx
        vncwindow.h
        vncconnection.h
        vncconnection.cxx
        vnccredential.cxx
        vnccredential.h
        abstractvncview.h
        abstractvncview.cxx
        keysym2ucs.c
        keysym2ucs.h
        gettext.h
        parameters.h
        parameters.cxx
        tunnelfactory.cxx
        tunnelfactory.h
        i18n.h
        menukey.h
        menukey.cxx
        MonitorIndicesParameter.cxx
        MonitorIndicesParameter.h
        PlatformPixelBuffer.h
        PlatformPixelBuffer.cxx
        Surface.h
        Surface.cxx
        EmulateMB.h
        EmulateMB.cxx
        CConn.h
        CConn.cxx
        qml.qrc
        ${OS_DEP_SOURCES}
)

if(BUILD_STATIC)
  add_definitions(-DQT_STATIC_BUILD=1)
endif()

if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND ((QT_VERSION_MAJOR GREATER_EQUAL 6) OR ((QT_VERSION_MAJOR GREATER_EQUAL 5) AND (QT_VERSION_MAJOR GREATER_EQUAL 15))))
  set_target_properties(vncviewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(WIN32)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})
  configure_file(vncviewer.rc.in vncviewer.rc)
endif()

if(WIN32)
  set(OS_DEPENDENT_LIBRARIES msimg32)
  if(BUILD_STATIC)
    set(OS_DEPENDENT_LIBRARIES ${OS_DEPENDENT_LIBRARIES} -static libzstd.a libstdc++.a -dynamic)
  endif()
elseif(APPLE)
  set(OS_DEPENDENT_LIBRARIES "-framework Cocoa -framework Carbon -framework IOKit -framework QuartzCore")
else()
  set(OS_DEPENDENT_LIBRARIES ${X11_XiLIB} "-lXi" "-lXrender" "-lXext" "-lX11")
  if(X11_Xrandr_LIB)
    add_definitions(-DHAVE_XRANDR)
    set(OS_DEPENDENT_LIBRARIES "${OS_DEPENDENT_LIBRARIES} ${X11_Xrandr_LIB}")
  endif()
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    if (WIN32)
      set(app_icon_resource_windows "${CMAKE_CURRENT_BINARY_DIR}/vncviewer.rc")
    endif()
    qt_add_executable(vncviewer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${app_icon_resource_windows}
    )
else()
    if(ANDROID)
        add_library(vncviewer SHARED
          ${PROJECT_SOURCES}
        )
    else()
        if (WIN32)
          set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_BINARY_DIR}/vncviewer.rc")
          add_executable(vncviewer
            ${PROJECT_SOURCES}
            ${APP_ICON_RESOURCE_WINDOWS}
          )
        endif()
        if(APPLE)
          set(MACOSX_BUNDLE_BUNDLE_NAME "TigerVNC Viewer")
          set(MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION}${BUILD}")
          set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 1998-2022 [many holders]")
          set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.tigervnc.tigervnc")
          set(MACOSX_BUNDLE_ICON_FILE tigervnc.icns)
          set(MACOSX_BUNDLE_INFO_STRING "${VERSION}, Copyright © 1998-2022 [many holders]")
          set(MACOSX_BUNDLE_LONG_VERSION_STRING "TigerVNC Viewer ${VERSION}")
          set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION}")
          set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/../media/icons/tigervnc.icns)
          set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
          add_executable(vncviewer
            ${PROJECT_SOURCES}
            ${APP_ICON_MACOSX}
          )
        endif()
        if(UNIX AND NOT APPLE)
          find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS X11Extras)
          set(OS_DEPENDENT_LIBRARIES ${OS_DEPENDENT_LIBRARIES} Qt${QT_VERSION_MAJOR}::X11Extras)
          add_executable(vncviewer
            ${PROJECT_SOURCES}
          )
        endif()
    endif()
endif()

if(APPLE)
    set_target_properties(vncviewer PROPERTIES MACOSX_BUNDLE TRUE)
endif()

target_compile_definitions(vncviewer
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_link_libraries(vncviewer
      PRIVATE
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Quick
      Qt${QT_VERSION_MAJOR}::Widgets
      rfb network rdr os ${GETTEXT_LIBRARIES} ${OS_DEPENDENT_LIBRARIES})
else()
    if(BUILD_STATIC)
        find_package(Qt5QmlImportScanner REQUIRED)
        qt5_import_qml_plugins(vncviewer)
        qt5_import_plugins(vncviewer INCLUDE Qt${QT_VERSION_MAJOR}::QSvgPlugin)
        target_link_libraries(vncviewer
            PRIVATE
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Qml
            Qt${QT_VERSION_MAJOR}::QmlWorkerScript
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::QuickControls2
            Qt${QT_VERSION_MAJOR}::Svg
            Qt${QT_VERSION_MAJOR}::ThemeSupport
            Qt${QT_VERSION_MAJOR}::Widgets
            rfb network rdr os ${GETTEXT_LIBRARIES} ${OS_DEPENDENT_LIBRARIES})
    else()
        target_link_libraries(vncviewer
            PRIVATE
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::Widgets
            rfb network rdr os ${GETTEXT_LIBRARIES} ${OS_DEPENDENT_LIBRARIES})
    endif()
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_import_qml_plugins(vncviewer)
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
        add_custom_command(
            TARGET vncviewer
            POST_BUILD
            COMMAND ${DEPLOYQT} ${RELDEB} --dir ${DEPLOY_DIR} --qmldir ${CMAKE_SOURCE_DIR}/vncviewer/qml --no-translations $<TARGET_FILE:vncviewer>)
    endif()

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
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:vncviewer>/../Resources/qml")
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
        COMMAND ${DEPLOYQT} "${DEPLOY_DIR}" "-dmg" "-always-overwrite" "-qmldir=${CMAKE_SOURCE_DIR}/vncviewer/qml" "-verbose=2")

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
