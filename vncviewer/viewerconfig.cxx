#include "viewerconfig.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QApplication>
#include <QDate>

#if !defined(WIN32)
#include <sys/stat.h>
#else
#include <QThread>
#endif
#if defined(__APPLE__)
#include "cocoa.h"

#include <Carbon/Carbon.h>
#endif
#ifdef Q_OS_LINUX
#include "x11utils.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif
#endif

#include "appmanager.h"
#include "parameters.h"
#include "rfb/Configuration.h"
#include "rfb/LogWriter.h"
#ifdef HAVE_GNUTLS
#include "rfb/CSecurityTLS.h"
#endif
#include "i18n.h"
#include "network/TcpSocket.h"
#include "rfb/Exception.h"
#undef asprintf

using namespace rfb;
using namespace std;

static LogWriter vlog("ViewerConfig");

ViewerConfig::ViewerConfig()
  : QObject(nullptr)
{

}

ViewerConfig *ViewerConfig::instance()
{
  static ViewerConfig config;
  return &config;
}

ViewerConfig::FullscreenType ViewerConfig::fullscreenType()
{
  if (!strcasecmp(::fullScreenMode.getValueStr().c_str(), "selected")) {
    return Selected;
  }

  if (!strcasecmp(::fullScreenMode.getValueStr().c_str(), "all")) {
    if (ViewerConfig::canFullScreenOnMultiDisplays()) {
      return All;
    }
  }

  return Current;
}

bool ViewerConfig::canFullScreenOnMultiDisplays()
{
#if defined(__APPLE__)
  return !cocoa_displays_have_separate_spaces();
#elif defined(Q_OS_LINUX)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  auto dpy = QX11Info::display();
#else
  auto dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  bool supported = X11Utils::isEWMHsupported(dpy);
  vlog.debug("isEWMHsupported %d", supported);
  bool wm = hasWM();
  return supported || !wm;
#else
  return true;
#endif
}

bool ViewerConfig::hasWM()
{
#if defined(Q_OS_LINUX)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto dpy = QX11Info::display();
#else
    auto dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  bool hasWM = X11Utils::hasWM(dpy);
  vlog.debug("hasWM %d", hasWM);
  return hasWM;
#else
  return true;
#endif
}
