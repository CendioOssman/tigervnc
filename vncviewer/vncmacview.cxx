#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QApplication>
#include <QEvent>
#include <QTextStream>
#include <QDataStream>
#include <QUrl>
#include <QWindow>
#include <QMenu>
#include <QImage>
#include <QBitmap>
#include <QLabel>
#include <QTimer>
#include <QAbstractEventDispatcher>
#include "rfb/ServerParams.h"
#include "rfb/LogWriter.h"
#include "rdr/Exception.h"
#include "rfb/ledStates.h"
#include "i18n.h"
#include "parameters.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"
#include "vncwindow.h"
#include "vncmacview.h"
#include "MacKeyboardHandler.h"

#include <QDebug>
#include <QMouseEvent>
#include <QScreen>

#include "cocoa.h"
extern const unsigned short code_map_osx_to_qnum[];
extern const unsigned int code_map_osx_to_qnum_len;

#ifndef XK_VoidSymbol
#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include <rfb/keysymdef.h>
#endif

#ifndef NoSymbol
#define NoSymbol 0
#endif

static rfb::LogWriter vlog("QVNCMacView");


QVNCMacView::QVNCMacView(QWidget *parent, Qt::WindowFlags f)
 : QAbstractVNCView(parent, f)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  setAttribute(Qt::WA_NoBackground);
#endif
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);

  keyboardHandler_ = new MacKeyboardHandler(this);
  initKeyboardHandler();
}

QVNCMacView::~QVNCMacView()
{
}

bool QVNCMacView::event(QEvent *e)
{
  switch(e->type()) {
  case QEvent::KeyboardLayoutChange:
    break;
  case QEvent::WindowActivate:
    //qDebug() << "WindowActivate";
    grabPointer();
    break;
  case QEvent::WindowDeactivate:
    //qDebug() << "WindowDeactivate";
    ungrabPointer();
    break;
  case QEvent::Enter:
  case QEvent::FocusIn:
    //qDebug() << "Enter/FocusIn";
    setFocus();
    grabPointer();
    break;
  case QEvent::Leave:
  case QEvent::FocusOut:
    //qDebug() << "Leave/FocusOut";
    clearFocus();
    ungrabPointer();
    break;
  case QEvent::CursorChange:
    //qDebug() << "CursorChange";
    e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    return true;
 case QEvent::MetaCall:
    // qDebug() << "QEvent::MetaCall (signal-slot call)";
    break;
  default:
    //qDebug() << "Unprocessed Event: " << e->type();
    break;
  }
  return QWidget::event(e);
}

void QVNCMacView::resizeEvent(QResizeEvent *e)
{
    QSize size = e->size();
    QWidget::resize(size.width(), size.height());

    // Try to get the remote size to match our window size, provided
    // the following conditions are true:
    //
    // a) The user has this feature turned on
    // b) The server supports it
    // c) We're not still waiting for startup fullscreen to kick in
    //
    QVNCConnection *cc = AppManager::instance()->connection();
    if (!firstUpdate_ && ViewerConfig::config()->remoteResize() && cc->server()->supportsSetDesktopSize) {
      postRemoteResizeRequest();
    }
    // Some systems require a grab after the window size has been changed.
    // Otherwise they might hold on to displays, resulting in them being unusable.
    maybeGrabKeyboard();
}

void QVNCMacView::bell()
{
  cocoa_beep();
}

void QVNCMacView::handleClipboardData(const char* data)
{
  if (!hasFocus()) {
    return;
  }

  size_t len = strlen(data);
  vlog.debug("Got clipboard data (%d bytes)", (int)len);
}

void QVNCMacView::fullscreenOnCurrentDisplay()
{
  QVNCWindow *window = AppManager::instance()->window();
  QScreen *screen = getCurrentScreen();
  window->windowHandle()->setScreen(screen);

  // Capture the fullscreen geometry.
  double dpr = effectiveDevicePixelRatio(screen);
  QRect vg = screen->geometry();
  fxmin_ = vg.x();
  fymin_ = vg.y();
  fw_ = vg.width() * dpr;
  fh_ = vg.height() * dpr;

  window->move(fxmin_, fymin_);
  window->resize(fw_, fh_);
  resize(fw_, fh_);
  window->setWindowFlag(Qt::FramelessWindowHint, true);
  window->setWindowState(Qt::WindowFullScreen);
  grabKeyboard();
}

void QVNCMacView::fullscreenOnSelectedDisplay(QScreen *screen, int vx, int vy, int vwidth, int vheight)
{
  QVNCWindow *window = AppManager::instance()->window();
  window->windowHandle()->setScreen(screen);
  window->move(vx, vy);
  window->resize(vwidth, vheight);
  resize(vwidth, vheight);
  window->setWindowFlag(Qt::FramelessWindowHint, true);
  window->setWindowState(Qt::WindowFullScreen);
  grabKeyboard();
}
