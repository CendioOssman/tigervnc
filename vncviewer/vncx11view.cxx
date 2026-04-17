#include "vncx11view.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QApplication>
#include <QBitmap>
#include <QGestureEvent>
#include <QGestureRecognizer>
#include <QImage>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif

#include "KeyboardX11.h"
#include "rfb/LogWriter.h"
#include "i18n.h"

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2.h>
#include <X11/XKBlib.h>

static rfb::LogWriter vlog("QVNCX11View");

QVNCX11View::QVNCX11View(CConn* cc_, QWidget* parent, Qt::WindowFlags f)
  : Viewport(cc_, parent, f)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  display = QX11Info::display();
#else
  display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
}
