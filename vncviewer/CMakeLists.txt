if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "cmake must be invoked with the top level directory")
endif()

include_directories(${GETTEXT_INCLUDE_DIR})

include_directories(${CMAKE_SOURCE_DIR}/common)

if (WIN32)
  set(APP_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/winvnc.ico")
  set(OS_DEP_SOURCES
      vncwinview.cxx
      vncwinview.h
      Surface_Win32.cxx
      win32.c
      win32.h
  )
elseif(APPLE)
#  set(APP_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/media/icons/tigervnc.icns")
  set(OS_DEP_SOURCES
      #vncosxview.cxx
      #vncosxview.h
      Surface_OSX.cxx
  )
else()
  set(OS_DEP_SOURCES
      vncx11view.cxx
      vncx11view.h
      Surface_X11.cxx
      #vncwaylandview.cxx
      #vncwaylandview.h
  )
endif()

set(PROJECT_SOURCES
        main.cxx
        appmanager.h
        appmanager.cxx
        eventfilter.h
        eventfilter.cxx
        msgreader.h
        msgreader.cxx
        msgwriter.h
        msgwriter.cxx
        qdesktopwindow.h
        qdesktopwindow.cxx
        viewerconfig.h
        viewerconfig.cxx
        vnccanvas.h
        vnccanvas.cxx
        vncconnection.h
        vncrenderer.cxx
        vncrenderer.h
        logorenderer.cxx
        logorenderer.h
        vncconnection.cxx
        vncpackethandler.h
        vncstream.h
        vncstream.cxx
        abstractvncview.h
        abstractvncview.cxx
        vncwinview.h
        vncwinview.cxx
        keysym2ucs.c
        keysym2ucs.h
        #
        gettext.h
        parameters.h
        parameters.cxx
        i18n.h
        menukey.h
        menukey.cxx
        MonitorIndicesParameter.cxx
        MonitorIndicesParameter.h
        PlatformPixelBuffer.h
        PlatformPixelBuffer.cxx
        PixelBuffer.cxx
        Surface.h
        Surface.cxx
        DecodeManager.h
        DecodeManager.cxx
        #
        ##CConn.h
        ##CConn.cxx
        qml.qrc
        ${APP_ICONS}
        ${OS_DEP_SOURCES}
)

if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  set_target_properties(vncviewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

###if(WIN32)
###  # Since vncviewer.rc is generated, local includes will be looking
###  # in the wrong directory. We need to help it out.
###  include_directories(${CMAKE_CURRENT_SOURCE_DIR})
###  configure_file(vncviewer.rc.in vncviewer.rc)
###  target_sources(vncviewer PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/vncviewer.rc)
###endif()

###if(WIN32)
###  target_sources(vncviewer PRIVATE Win32TouchHandler.cxx win32.c)
###elseif(APPLE)
###  target_sources(vncviewer PRIVATE cocoa.mm osx_to_qnum.c)
###else()
###  target_sources(vncviewer PRIVATE GestureHandler.cxx XInputTouchHandler.cxx xkb_to_qnum.c)
###endif()

###if(WIN32)
###  target_sources(vncviewer PRIVATE Surface_Win32.cxx)
###elseif(APPLE)
###  target_sources(vncviewer PRIVATE Surface_OSX.cxx)
###else()
###  target_sources(vncviewer PRIVATE Surface_X11.cxx)
###endif()

if(WIN32)
  set(OS_DEPENDENT_LIBRARIES msimg32)
elseif(APPLE)
  set(OS_DEPENDENT_LIBRARIES "-framework Cocoa -framework Carbon -framework IOKit")
else()
  set(OS_DEPENDENT_LIBRARIES ${X11_Xi_LIB})

  if(X11_Xrandr_LIB)
    add_definitions(-DHAVE_XRANDR)
    set(OS_DEPENDENT_LIBRARIES "${OS_DEPENDENT_LIBRARIES} ${X11_Xrandr_LIB}")
  endif()
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    set(app_icon_resource_windows "${CMAKE_CURRENT_SOURCE_DIR}/vncviewer.rc")
    qt_add_executable(vncviewer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${app_icon_resource_windows}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET vncviewer APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(vncviewer SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        if (WIN32)
          set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/vncviewer.rc")
          find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS WinExtras)
        endif()
        add_executable(vncviewer
          ${PROJECT_SOURCES}
          ${APP_ICON_RESOURCE_WINDOWS}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS OpenGL)

target_compile_definitions(vncviewer
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    target_link_libraries(vncviewer
      PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::QuickWidgets Qt${QT_VERSION_MAJOR}::OpenGL Qt${QT_VERSION_MAJOR}::Widgets rfb network rdr os ${GETTEXT_LIBRARIES} ${OS_DEPENDENT_LIBRARIES}) # ${Intl_LIBRARIES}
else()
    target_link_libraries(vncviewer
      PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::OpenGL Qt${QT_VERSION_MAJOR}::WinExtras Qt${QT_VERSION_MAJOR}::Widgets rfb network rdr os ${GETTEXT_LIBRARIES} ${OS_DEPENDENT_LIBRARIES}) # ${Intl_LIBRARIES}
endif()

if(${QT_VERSION_MAJOR} EQUAL 6)
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

if(WIN32 OR APPLE)
    set(DEPLOY_DIR ${CMAKE_SOURCE_DIR}/deploy CACHE PATH "Package directory")
    get_target_property(LOCAL_QMAKE_PATH Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(LOCAL_QTBIN_ATH ${LOCAL_QMAKE_PATH} DIRECTORY)
    if(WIN32)
        find_program(DEPLOYQT NAMES windeployqt HINTS "${LOCAL_QTBIN_ATH}")
    else()
        find_program(DEPLOYQT NAMES macdeployqt HINTS "${LOCAL_QTBIN_ATH}")
    endif()
    add_custom_command(
        TARGET vncviewer
        POST_BUILD
        COMMAND ${DEPLOYQT} --$<IF:$<CONFIG:Debug>,debug,release> --dir "${DEPLOY_DIR}" --qmldir ${CMAKE_SOURCE_DIR}/vncviewer/qml $<TARGET_FILE_DIR:vncviewer>/$<TARGET_FILE_NAME:vncviewer>)
    add_custom_command(
        TARGET vncviewer
        POST_BUILD
        COMMAND cp -f $<TARGET_FILE_DIR:vncviewer>/$<TARGET_FILE_NAME:vncviewer> "${DEPLOY_DIR}")
    # To execute mac/windeployqt on 'cmake --install', uncomment the following line.
    #install(CODE "execute_process(COMMAND ${DEPLOYQT} --$<IF:$<CONFIG:Debug>,debug,release> --dir "${DEPLOY_DIR}" --qmldir ${CMAKE_SOURCE_DIR}/vncviewer/qml ${CMAKE_INSTALL_PREFIX}/$<TARGET_FILE_NAME:vncviewer>)")
endif()
