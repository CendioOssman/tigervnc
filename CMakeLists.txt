#
# Setup
#

cmake_minimum_required(VERSION 3.10.0)

# Internal cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)
include(CheckCSourceRuns)

include(CMakeMacroLibtoolFile)

project(tigervnc)
set(VERSION 1.16.80)

# The RC version must always be four comma-separated numbers
string(REPLACE . , RCVERSION "${VERSION}.0")

# Installation paths
include(GNUInstallDirs)
set(CMAKE_INSTALL_UNITDIR "lib/systemd/system" CACHE PATH "systemd unit files (lib/systemd/system)")
if(IS_ABSOLUTE "${CMAKE_INSTALL_UNITDIR}")
  set(CMAKE_INSTALL_FULL_UNITDIR "${CMAKE_INSTALL_UNITDIR}")
else()
  set(CMAKE_INSTALL_FULL_UNITDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_UNITDIR}")
endif()

option(INSTALL_SYSTEMD_UNITS "Install TigerVNC systemd units" ON)

if(MSVC)
  include_directories(BEFORE SYSTEM ${CMAKE_SOURCE_DIR}/contrib/msvc/include)
  add_compile_options(/FI msvc_compat.h)
endif()

if(NOT BUILD_TIMESTAMP)
  STRING(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M" UTC)
endif()

# Default to optimised builds instead of debug ones. Our code has no bugs ;)
# (CMake makes it fairly easy to toggle this back to Debug if needed)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

message(STATUS "VERSION = ${VERSION}")
message(STATUS "BUILD_TIMESTAMP = ${BUILD_TIMESTAMP}")

# We want to keep our asserts even in release builds so remove NDEBUG
add_definitions(-UNDEBUG)

# But extra debug checks are still gated by this custom define
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-D_DEBUG)
endif()

# Enable debug friendly optimizations for debug builds
if(NOT MSVC)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og")
endif()

# Make sure we get a sane C and C++ version
if(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /std:c17")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu17")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17")
endif()

# Tell the compiler to be stringent
if(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
else()
  add_compile_definitions(_FORTIFY_SOURCE=2)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat=2 -Wvla")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat=2 -Wvla")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wzero-as-null-pointer-constant")
  check_cxx_compiler_flag("-Woverloaded-virtual=2" HAVE_WOVERLOADED_VIRTUAL)
  if(HAVE_WOVERLOADED_VIRTUAL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual=2")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsuggest-override")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
endif()
# Make sure we catch these issues whilst developing
if(MSVC)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /WX")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /WX")
else()
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Werror")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Werror")
endif()

option(ENABLE_ASAN "Enable address sanitizer support" OFF)
if(ENABLE_ASAN AND NOT WIN32 AND NOT APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

option(ENABLE_TSAN "Enable thread sanitizer support" OFF)
if(ENABLE_TSAN AND NOT WIN32 AND NOT APPLE AND CMAKE_SIZEOF_VOID_P MATCHES 8)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

if(NOT DEFINED BUILD_WINVNC)
  set(BUILD_WINVNC 1)
endif()

# Minimum version is Windows 10
if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0A00)
endif()

# Legacy macros (macOS 10.12 and older) conflict with our code
if(APPLE)
  add_definitions(-D__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES=0)
endif()

#### Check for required and optional libraries ####

macro(trioption VAR DESC)
  set(${VAR} AUTO CACHE STRING "${DESC}")
  set_property(CACHE ${VAR} PROPERTY STRINGS AUTO ON OFF)
endmacro()

# X11 stuff. It's in a if() so that we can say REQUIRED
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  if(X11_Xdamage_LIB)
    set(HAVE_XDAMAGE 1)
  endif()
  if(X11_Xfixes_LIB)
    set(HAVE_XFIXES 1)
  endif()
  if(X11_Xrandr_LIB)
    set(HAVE_XRANDR 1)
  endif()
  if(X11_XTest_LIB)
    set(HAVE_XTEST 1)
  endif()
endif()

# Check for zlib
find_package(ZLIB REQUIRED)

# Check for pixman
find_package(Pixman REQUIRED)

# Check for gettext
trioption(ENABLE_NLS "Enable translation of program messages")
if(ENABLE_NLS)
  # Tools
  if(ENABLE_NLS STREQUAL "AUTO")
    find_package(Gettext)
  else()
    find_package(Gettext REQUIRED)
  endif()

  # Runtime library
  if(ENABLE_NLS STREQUAL "AUTO")
    find_package(Intl)
  else()
    find_package(Intl REQUIRED)
  endif()

  if(NOT GETTEXT_FOUND OR NOT INTL_FOUND)
    message(WARNING "Gettext NOT found.  Native Language Support disabled.")
    set(ENABLE_NLS 0)
  endif()
endif()

trioption(ENABLE_H264 "Enable H.264 RFB encoding")
if(ENABLE_H264)
  if(WIN32)
    set(HAVE_H264 1)
  else()
    if(ENABLE_H264 STREQUAL "AUTO")
      find_package(AVCodec)
      find_package(AVUtil)
      find_package(SWScale)
    else()
      find_package(AVCodec REQUIRED)
      find_package(AVUtil REQUIRED)
      find_package(SWScale REQUIRED)
    endif()
    if (AVCODEC_FOUND AND AVUTIL_FOUND AND SWSCALE_FOUND)
      set(HAVE_H264 1)
      set(HAVE_LIBAV 1)
    else()
      message(WARNING "FFMPEG support can't be found")
    endif()
  endif()
endif()

trioption(ENABLE_AUDIO "Enable audio playback in the viewer")
if(ENABLE_AUDIO)
  if(WIN32)
    # waveOut comes with the OS
    set(HAVE_AUDIO 1)
  elseif(APPLE)
    if(ENABLE_AUDIO STREQUAL "AUTO")
      message(WARNING "Audio playback is not supported on macOS")
    else()
      message(FATAL_ERROR "Audio playback is not supported on macOS")
    endif()
  else()
    if(ENABLE_AUDIO STREQUAL "AUTO")
      find_package(Pulse)
    else()
      find_package(Pulse REQUIRED)
    endif()
    if(PULSE_FOUND)
      set(HAVE_AUDIO 1)
    else()
      message(WARNING "PulseAudio NOT found. Audio playback disabled.")
    endif()
  endif()
endif()

# Check for libjpeg
find_package(JPEG REQUIRED)

# Warn if it doesn't seem to be the accelerated libjpeg that's found
set(CMAKE_REQUIRED_LIBRARIES ${JPEG_LIBRARIES})
set(CMAKE_REQUIRED_FLAGS -I${JPEG_INCLUDE_DIR})

set(JPEG_TEST_SOURCE "\n
  #include <stdio.h>\n
  #include <jpeglib.h>\n
  int main(void) {\n
    struct jpeg_compress_struct cinfo;\n
    struct jpeg_error_mgr jerr;\n
    cinfo.err=jpeg_std_error(&jerr);\n
    jpeg_create_compress(&cinfo);\n
    cinfo.input_components = 3;\n
    jpeg_set_defaults(&cinfo);\n
    cinfo.in_color_space = JCS_EXT_RGB;\n
    jpeg_default_colorspace(&cinfo);\n
    return 0;\n
  }")

if(CMAKE_CROSSCOMPILING)
  check_c_source_compiles("${JPEG_TEST_SOURCE}" FOUND_LIBJPEG_TURBO)
else()
  check_c_source_runs("${JPEG_TEST_SOURCE}" FOUND_LIBJPEG_TURBO)
endif()

set(CMAKE_REQUIRED_LIBRARIES)
set(CMAKE_REQUIRED_FLAGS)
set(CMAKE_REQUIRED_DEFINITIONS)

if(NOT FOUND_LIBJPEG_TURBO)
  message(STATUS "Warning: You are not using libjpeg-turbo. Performance will suffer.")
endif()

option(BUILD_JAVA "Build Java version of the TigerVNC Viewer" FALSE)
if(BUILD_JAVA)
  add_subdirectory(java)
endif()

trioption(BUILD_VIEWER "Build TigerVNC viewer")
if(BUILD_VIEWER)
  # We maintain Qt 5.15 compatibility for RHEL 8
  if(DEFINED QT_SELECT)
    set(QT_VERSION_MAJOR ${QT_SELECT})
  else()
    find_package(QT NAMES Qt6 COMPONENTS Core)
    if(NOT DEFINED QT_VERSION_MAJOR)
      find_package(QT NAMES Qt5 COMPONENTS Core)
    endif()
    if(NOT DEFINED QT_VERSION_MAJOR)
      set(QT_VERSION_MAJOR 6)
    endif()
  endif()

  if(BUILD_VIEWER STREQUAL "AUTO")
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network Widgets)
  else()
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network Widgets REQUIRED)
  endif()

  if(Qt${QT_VERSION_MAJOR}_FOUND)
    if(UNIX AND NOT APPLE)
      if(QT_VERSION_MAJOR LESS_EQUAL 5)
        if(BUILD_VIEWER STREQUAL "AUTO")
          find_package(Qt${QT_VERSION_MAJOR} COMPONENTS X11Extras)
        else()
          find_package(Qt${QT_VERSION_MAJOR} COMPONENTS X11Extras REQUIRED)
        endif()
      endif()
    endif()
  endif()

  add_definitions("-DQT_DISABLE_DEPRECATED_BEFORE=0x050f00")
  add_definitions("-DQT_DEPRECATED_WARNINGS_SINCE=0x050f00")

  if(NOT Qt${QT_VERSION_MAJOR}_FOUND)
    message(WARNING "Qt NOT found. TigerVNC viewer disabled.")
    set(BUILD_VIEWER 0)
  endif()
endif()
if(BUILD_VIEWER)
  if(UNIX AND NOT APPLE)
    if(BUILD_VIEWER STREQUAL "AUTO")
      find_package(X11 COMPONENTS Xi Xinerama Xkb)
      find_package(XCB COMPONENTS XKB)
    else()
      find_package(X11 COMPONENTS Xi Xinerama Xkb REQUIRED)
      find_package(XCB COMPONENTS XKB REQUIRED)
    endif()

    if(NOT X11_FOUND)
      message(WARNING "X11 libraries NOT found. TigerVNC viewer disabled.")
      set(BUILD_VIEWER 0)
    endif()
    if(NOT XCB_FOUND)
      message(WARNING "xcb libraries NOT found. TigerVNC viewer disabled.")
      set(BUILD_VIEWER 0)
    endif()
  endif()
endif()

# Check for GNUTLS library
trioption(ENABLE_GNUTLS "Enable protocol encryption and advanced authentication")
if(ENABLE_GNUTLS)
  if(ENABLE_GNUTLS STREQUAL "AUTO")
    find_package(GnuTLS)
  else()
    find_package(GnuTLS REQUIRED)
  endif()
  if (GNUTLS_FOUND)
    set(HAVE_GNUTLS 1)
  endif()
endif()

trioption(ENABLE_NETTLE "Enable RSA-AES security types")
if (ENABLE_NETTLE)
  if(ENABLE_NETTLE STREQUAL "AUTO")
    find_package(Nettle)
  else()
    find_package(Nettle REQUIRED)
  endif()
  if (NETTLE_FOUND)
    set(HAVE_NETTLE 1)
  endif()
endif()

# Check for PAM library
if(UNIX AND NOT APPLE)
  trioption(ENABLE_PAM "Enable authentication using PAM")
  if(ENABLE_PAM)
    if(ENABLE_PAM STREQUAL "AUTO")
      find_package(PAM)
    else()
      find_package(PAM REQUIRED)
    endif()
    if(PAM_FOUND)
      set(HAVE_PAM 1)
    else()
      message(WARNING "PAM NOT found. vncserver and PAM authentication disabled.")
    endif()
  endif()
endif()

# Check for SELinux library
if(UNIX AND NOT APPLE)
  trioption(ENABLE_SELINUX "Enable SELinux support")
  if(ENABLE_SELINUX)
    if(ENABLE_SELINUX STREQUAL "AUTO")
      find_package(SELinux)
    else()
      find_package(SELinux REQUIRED)
    endif()
    if(SELINUX_FOUND)
      set(HAVE_SELINUX 1)
    endif()
  endif()
endif()

# check for systemd support (socket activation)
if(UNIX AND NOT APPLE)
  trioption(ENABLE_SYSTEMD "Enable systemd support")
  if(ENABLE_SYSTEMD)
    if(ENABLE_SYSTEMD STREQUAL "AUTO")
      find_package(Systemd)
    else()
      find_package(Systemd REQUIRED)
    endif()
    if (SYSTEMD_FOUND)
      set(HAVE_LIBSYSTEMD 1)
    endif()
  endif()
endif()

# check for password pwquality check support
if(UNIX AND NOT APPLE)
  trioption(ENABLE_PWQUALITY "Enable password pwquality check")
  if(ENABLE_PWQUALITY)
    if(ENABLE_PWQUALITY STREQUAL "AUTO")
      find_package(PWQuality)
    else()
      find_package(PWQuality REQUIRED)
    endif()
    if(PWQUALITY_FOUND)
      set(HAVE_PWQUALITY 1)
    endif()
  endif()
endif()

# check for libraries needed for wayland support
if(UNIX AND NOT APPLE)
  trioption(ENABLE_WAYLAND "Enable wayland support")
  if(ENABLE_WAYLAND)
    if(ENABLE_WAYLAND STREQUAL "AUTO")
      find_package(GLib)
      find_package(Gio)
      find_package(Gobject)

      # Portals specific
      find_package(PipeWire)
      find_package(Uuid)

      # wlroots specific
      find_package(WaylandClient)
      find_package(Xkbcommon)
    else()
      find_package(GLib REQUIRED)
      find_package(Gio REQUIRED)
      find_package(Gobject REQUIRED)

      # Portals specific
      find_package(PipeWire REQUIRED)
      find_package(Uuid REQUIRED)

      # wlroots specific
      find_package(WaylandClient REQUIRED)
      find_package(Xkbcommon REQUIRED)
    endif()
    if(NOT GLIB_FOUND OR NOT GIO_FOUND OR NOT GOBJECT_FOUND OR NOT PIPEWIRE_FOUND
       OR NOT UUID_FOUND OR NOT WAYLANDCLIENT_FOUND OR NOT XKBCOMMON_FOUND)
      set(ENABLE_WAYLAND 0)
      message(WARNING "GLib, Gio, Gobject, PipeWire, Uuid, WaylandClient or Xkbcommon NOT found. w0vncserver disabled.")
    endif()
  endif()
endif()

find_package(GTest)

# Generate config.h and make sure the source finds it
configure_file(config.h.in config.h)
add_definitions(-DHAVE_CONFIG_H)
include_directories(${CMAKE_BINARY_DIR})

include(cmake/StaticBuild.cmake)

add_subdirectory(common)

if(WIN32)
  add_subdirectory(win)
else()
  # No interest in building x related parts on Apple
  if(NOT APPLE)
    add_subdirectory(unix)
  endif()
endif()

if(ENABLE_NLS)
  add_subdirectory(po)
endif()

if(BUILD_VIEWER)
  add_subdirectory(vncviewer)
  add_subdirectory(media)
endif()

add_subdirectory(tests)


if(BUILD_VIEWER)
  add_subdirectory(release)
endif()

# uninstall
configure_file("${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
  "cmake_uninstall.cmake" IMMEDIATE @ONLY)

add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P cmake_uninstall.cmake)

libtool_generate_control_files()
