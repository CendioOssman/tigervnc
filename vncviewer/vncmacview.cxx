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
 , cursor_(nullptr)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  setAttribute(Qt::WA_NoBackground);
#endif
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);
}

QVNCMacView::~QVNCMacView()
{
}

bool QVNCMacView::event(QEvent *e)
{
  switch(e->type()) {
  case QEvent::KeyboardLayoutChange:
    break;
  case QEvent::MouseMove:
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease:
  case QEvent::MouseButtonDblClick:
    //qDebug() << "QVNCMacView::event: MouseButton event";
    handleMouseButtonEvent((QMouseEvent*)e);
    break;
  case QEvent::Wheel:
    handleMouseWheelEvent((QWheelEvent*)e);
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

void QVNCMacView::showEvent(QShowEvent *e)
{
  QWidget::showEvent(e);
}

void QVNCMacView::focusInEvent(QFocusEvent *e)
{
  qDebug() << "QVNCMacView::focusInEvent";
  maybeGrabKeyboard();
  disableIM();

  //flushPendingClipboard();

  // We may have gotten our lock keys out of sync with the server
  // whilst we didn't have focus. Try to sort this out.
  pushLEDState();

  // Resend Ctrl/Alt if needed
  if (menuCtrlKey_) {
    handleKeyPress(0x1d, XK_Control_L);
  }
  if (menuAltKey_) {
    handleKeyPress(0x38, XK_Alt_L);
  }
  QWidget::focusInEvent(e);
}

void QVNCMacView::focusOutEvent(QFocusEvent *e)
{
  qDebug() << "QVNCMacView::focusOutEvent";
  if (ViewerConfig::config()->fullscreenSystemKeys()) {
    ungrabKeyboard();
  }
  // We won't get more key events, so reset our knowledge about keys
  resetKeyboard();
  enableIM();
  QWidget::focusOutEvent(e);
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

// Viewport::handle(int event)
void QVNCMacView::handleMouseButtonEvent(QMouseEvent *e)
{
  int buttonMask = 0;
  Qt::MouseButtons buttons = e->buttons();
  if (buttons & Qt::LeftButton) {
    buttonMask |= 1;
  }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  if (buttons & Qt::MidButton) {
#else
  if (buttons & Qt::MiddleButton) {
#endif
    buttonMask |= 2;
  }
  if (buttons & Qt::RightButton) {
    buttonMask |= 4;
  }

  //qDebug() << "handleMouseButtonEvent: x=" << e->x() << ",y=" << e->y();
  filterPointerEvent(rfb::Point(e->x(), e->y()), buttonMask);
}

// Viewport::handle(int event)
void QVNCMacView::handleMouseWheelEvent(QWheelEvent *e)
{
  int buttonMask = 0;
  Qt::MouseButtons buttons = e->buttons();
  if (buttons & Qt::LeftButton) {
    buttonMask |= 1;
  }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  if (buttons & Qt::MidButton) {
#else
  if (buttons & Qt::MiddleButton) {
#endif
    buttonMask |= 2;
  }
  if (buttons & Qt::RightButton) {
    buttonMask |= 4;
  }

  int wheelMask = 0;
  QPoint delta = e->angleDelta();
  int dy = delta.y();
  int dx = delta.x();
  if (dy < 0) {
    wheelMask |= 8;
  }
  if (dy > 0) {
    wheelMask |= 16;
  }
  if (dx < 0) {
    wheelMask |= 32;
  }
  if (dx > 0) {
    wheelMask |= 64;
  }

  // A quick press of the wheel "button", followed by a immediate
  // release below
  #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  filterPointerEvent(rfb::Point(e->position().x(), e->position().y()), buttonMask | wheelMask);
  #else
  filterPointerEvent(rfb::Point(e->x(), e->y()), buttonMask | wheelMask);
  #endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  filterPointerEvent(rfb::Point(e->position().x(), e->position().y()), buttonMask);
#else
  filterPointerEvent(rfb::Point(e->x(), e->y()), buttonMask);
#endif
}

void QVNCMacView::disableIM()
{
  // Seems nothing to do.
}

void QVNCMacView::enableIM()
{
  // Seems nothing to do.
}

void QVNCMacView::handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode)
{
  if (menuKeySym_ && keySym == menuKeySym_) {
    if (isVisibleContextMenu()) {
      if (!menuShortCutMode) {
        sendContextMenuKey();
        return;
      }
    }
    else {
      popupContextMenu();
    }
    return;
  }

  if (ViewerConfig::config()->viewOnly())
    return;

  if (keyCode == 0) {
    vlog.error(_("No key code specified on key press"));
    return;
  }

  // Alt on OS X behaves more like AltGr on other systems, and to get
  // sane behaviour we should translate things in that manner for the
  // remote VNC server. However that means we lose the ability to use
  // Alt as a shortcut modifier. Do what RealVNC does and hijack the
  // left command key as an Alt replacement.
  switch (keySym) {
  case XK_Super_L:
    keySym = XK_Alt_L;
    break;
  case XK_Super_R:
    keySym = XK_Super_L;
    break;
  case XK_Alt_L:
    keySym = XK_Mode_switch;
    break;
  case XK_Alt_R:
    keySym = XK_ISO_Level3_Shift;
    break;
  }

  // Because of the way keyboards work, we cannot expect to have the same
  // symbol on release as when pressed. This breaks the VNC protocol however,
  // so we need to keep track of what keysym a key _code_ generated on press
  // and send the same on release.
  downKeySym_[keyCode] = keySym;

  //  vlog.debug("Key pressed: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(keySym), keySym);

  try {
    QVNCConnection *cc = AppManager::instance()->connection();
    // Fake keycode?
    if (keyCode > 0xff)
      emit cc->writeKeyEvent(keySym, 0, true);
    else
      emit cc->writeKeyEvent(keySym, keyCode, true);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }
}

void QVNCMacView::handleKeyRelease(int keyCode)
{
  DownMap::iterator iter;

  if (ViewerConfig::config()->viewOnly())
    return;

  iter = downKeySym_.find(keyCode);
  if (iter == downKeySym_.end()) {
    // These occur somewhat frequently so let's not spam them unless
    // logging is turned up.
    vlog.debug("Unexpected release of key code %d", keyCode);
    return;
  }

  //  vlog.debug("Key released: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(iter->second), iter->second);

  try {
    QVNCConnection *cc = AppManager::instance()->connection();
    if (keyCode > 0xff)
      emit cc->writeKeyEvent(iter->second, 0, false);
    else
      emit cc->writeKeyEvent(iter->second, keyCode, false);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }

  downKeySym_.erase(iter);
}

void QVNCMacView::handleClipboardData(const char* data)
{
  if (!hasFocus()) {
    return;
  }

  size_t len = strlen(data);
  vlog.debug("Got clipboard data (%d bytes)", (int)len);
}

void QVNCMacView::setLEDState(unsigned int state)
{
  //qDebug() << "QVNCMacView::setLEDState";
  vlog.debug("Got server LED state: 0x%08x", state);

  // The first message is just considered to be the server announcing
  // support for this extension. We will push our state to sync up the
  // server when we get focus. If we already have focus we need to push
  // it here though.
  if (firstLEDState_) {
    firstLEDState_ = false;
    if (hasFocus()) {
      pushLEDState();
    }
    return;
  }

  if (!hasFocus()) {
    return;
  }

  int ret = cocoa_set_caps_lock_state(state & rfb::ledCapsLock);
  if (ret == 0) {
    ret = cocoa_set_num_lock_state(state & rfb::ledNumLock);
  }
  
  if (ret != 0) {
    vlog.error(_("Failed to update keyboard LED state: %d"), ret);
  }
}

void QVNCMacView::pushLEDState()
{
  //qDebug() << "QVNCMacView::pushLEDState";
  QVNCConnection *cc = AppManager::instance()->connection();
  // Server support?
  rfb::ServerParams *server = AppManager::instance()->connection()->server();
  if (server->ledState() == rfb::ledUnknown) {
    return;
  }

  bool on;
  int ret = cocoa_get_caps_lock_state(&on);
  if (ret != 0) {
    vlog.error(_("Failed to get keyboard LED state: %d"), ret);
    return;
  }
  unsigned int state = 0;
  if (on) {
    state |= rfb::ledCapsLock;
  }
  ret = cocoa_get_num_lock_state(&on);
  if (ret != 0) {
    vlog.error(_("Failed to get keyboard LED state: %d"), ret);
    return;
  }
  if (on) {
    state |= rfb::ledNumLock;
  }
  // No support for Scroll Lock //
  state |= (cc->server()->ledState() & rfb::ledScrollLock);
  if ((state & rfb::ledCapsLock) != (cc->server()->ledState() & rfb::ledCapsLock)) {
    vlog.debug("Inserting fake CapsLock to get in sync with server");
    handleKeyPress(0x3a, XK_Caps_Lock);
    handleKeyRelease(0x3a);
  }
  if ((state & rfb::ledNumLock) != (cc->server()->ledState() & rfb::ledNumLock)) {
    vlog.debug("Inserting fake NumLock to get in sync with server");
    handleKeyPress(0x45, XK_Num_Lock);
    handleKeyRelease(0x45);
  }
  if ((state & rfb::ledScrollLock) != (cc->server()->ledState() & rfb::ledScrollLock)) {
    vlog.debug("Inserting fake ScrollLock to get in sync with server");
    handleKeyPress(0x46, XK_Scroll_Lock);
    handleKeyRelease(0x46);
  }
}

void QVNCMacView::grabKeyboard()
{
  QAbstractVNCView::grabKeyboard();
}

void QVNCMacView::ungrabKeyboard()
{
  QAbstractVNCView::ungrabKeyboard();
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
