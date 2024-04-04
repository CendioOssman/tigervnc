#include "vncx11view.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
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

#include "X11KeyboardHandler.h"
#include "rfb/LogWriter.h"

#include <X11/XKBlib.h>

static rfb::LogWriter vlog("QVNCX11View");

QVNCX11View::QVNCX11View(QWidget* parent, Qt::WindowFlags f)
  : QAbstractVNCView(parent, f)
{
  keyboardHandler = new X11KeyboardHandler(this);
  initKeyboardHandler();
}

void QVNCX11View::bell()
{

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  auto display = QX11Info::display();
#else
  auto display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  XBell(display, 0 /* volume */);
}
