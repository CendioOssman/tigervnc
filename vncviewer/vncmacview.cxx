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

QVNCMacView::MacEventFilter::MacEventFilter(QVNCMacView *view)
 : view_(view)
{
}

QVNCMacView::MacEventFilter::~MacEventFilter()
{
}

/**
* Native event handler must be installed with QAbstractEventDispatcher::installNativeEventFilter()
* because QWidget::nativeEvent() doesn't get called on macOS if the widget does not have a native
* window handle. See QTBUG-40116 or QWidget document (https://doc.qt.io/qt-6.4/qwidget.html#nativeEvent)
* for more details.
* 
* @param eventType 
* @param message 
* @param result 
* 
* @return 
*/
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool QVNCMacView::MacEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#else
bool QVNCMacView::MacEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#endif
{
  Q_UNUSED(result)
  if (eventType == "mac_generic_NSEvent") {
    if (QApplication::activePopupWidget()) { // F8 popup menu
      return false;
    }
    if (QApplication::activeWindow() != view_->window()) { // QML dialog windows
      return false;
    }
#if 0
    if (cocoa_is_mouse_entered(message)) {
      qDebug() << "nativeEvent: mouseEntered";
      view_->setFocus();
      view_->grabPointer();
      return true;
    }
    if (cocoa_is_mouse_exited(message)) {
      qDebug() << "nativeEvent: mouseExited";
      view_->clearFocus();
      view_->ungrabPointer();
      return true;
    }
#endif
    if (cocoa_is_keyboard_sync(message)) {
      while (!view_->downKeySym_.empty()) {
        view_->handleKeyRelease(view_->downKeySym_.begin()->first);
      }
      return true;
    }
    if (cocoa_is_keyboard_event(message)) {
      int keyCode = cocoa_event_keycode(message);
      //qDebug() << "nativeEvent: keyEvent: keyCode=" << keyCode << ", hexKeyCode=" << Qt::hex << keyCode;
      if ((unsigned)keyCode >= code_map_osx_to_qnum_len) {
        keyCode = 0;
      }
      else {
        keyCode = code_map_osx_to_qnum[keyCode];
      }
      if (cocoa_is_key_press(message)) {
        uint32_t keySym = cocoa_event_keysym(message);
        if (keySym == NoSymbol) {
          vlog.error(_("No symbol for key code 0x%02x (in the current state)"), (int)keyCode);
        }

        view_->handleKeyPress(keyCode, keySym);

        // We don't get any release events for CapsLock, so we have to
        // send the release right away.
        if (keySym == XK_Caps_Lock) {
          view_->handleKeyRelease(keyCode);
        }
      }
      else {
        view_->handleKeyRelease(keyCode);
      }
      return true;
    }
  }
  return false;
}

QVNCMacView::QVNCMacView(QWidget *parent, Qt::WindowFlags f)
 : QAbstractVNCView(parent, f)
 , view_(0)
 , cursor_(nullptr)
 , filter_(nullptr)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  setAttribute(Qt::WA_NoBackground);
#endif
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_AcceptTouchEvents);
  setAttribute(Qt::WA_NativeWindow);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance()->connection(), &QVNCConnection::framebufferResized, this, [this](int width, int height) {
    Q_UNUSED(width)
    Q_UNUSED(height)
    PlatformPixelBuffer *framebuffer = (PlatformPixelBuffer*)AppManager::instance()->connection()->framebuffer();
    cocoa_resize(view_, framebuffer->bitmap());
  }, Qt::QueuedConnection);
}

QVNCMacView::~QVNCMacView()
{
  // cursor_ is autorelease.
  QAbstractEventDispatcher::instance()->removeNativeEventFilter(filter_);
  delete filter_;
}

qulonglong QVNCMacView::nativeWindowHandle() const
{
  return (qulonglong)view_;
}

void QVNCMacView::updateWindow()
{
  QAbstractVNCView::updateWindow();
  draw();
}

void QVNCMacView::installNativeEventHandler()
{
  QAbstractEventDispatcher::instance()->removeNativeEventFilter(filter_);
  delete filter_;

  filter_ = new MacEventFilter(this);
  QAbstractEventDispatcher::instance()->installNativeEventFilter(filter_);
}

bool QVNCMacView::event(QEvent *e)
{
  switch(e->type()) {
  case QEvent::Polish:
    if (!view_) {
      //qDebug() << "display numbers:  QMACInfo::display()=" <<  QMACInfo::display() << ", XOpenDisplay(NULL)=" << XOpenDisplay(NULL);
      QVNCConnection *cc = AppManager::instance()->connection();
      PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
      CGImage *bitmap = framebuffer->bitmap();
      view_ = cocoa_create_view(this, bitmap);
      // Do not invoke #fromWinId(), otherwise NSView won't be shown.
      //QWindow *w = windowHandle()->fromWinId((WId)view_);
      installNativeEventHandler();
      setMouseTracking(true);
    }
    break;
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
  QWidget::focusInEvent(e);
}

void QVNCMacView::resizeEvent(QResizeEvent *e)
{
  if (view_) {
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
}

void QVNCMacView::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event)
  draw();
}

void QVNCMacView::bell()
{
  cocoa_beep();
}

void QVNCMacView::draw()
{
  if (!view_ || !AppManager::instance()->view()) {
    return;
  }
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect rect = framebuffer->getDamage();
  int x = rect.tl.x;
  int y = rect.tl.y;
  int w = rect.br.x - x;
  int h = rect.br.y - y;
  if (!rect.is_empty()) {
    cocoa_draw(view_, x, y, w, h);
  }
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

void QVNCMacView::setQCursor(const QCursor &cursor)
{
  cursor_ = cocoa_set_cursor(view_, &cursor);
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
  int ret = cocoa_capture_displays(view_, fullscreenScreens());
  if (ret != 0) {
    vlog.error(_("Failure grabbing keyboard"));
    return;
  }
  QAbstractVNCView::grabKeyboard();
}

void QVNCMacView::ungrabKeyboard()
{
  cocoa_release_displays(view_, fullscreenEnabled_);
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
#if 0
  window->show();
#endif
  //popupContextMenu();
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

#if 0
  QTimer t;
  t.setInterval(1000);
  connect(&t, &QTimer::timeout, this, [this]() {
    contextMenu_->hide();
  });
  t.start();
#endif
  //popupContextMenu();
}
