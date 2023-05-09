#include <QEvent>
#include <QTextStream>
#include <QDataStream>
#include <QUrl>
#include <QWindow>
#include <QImage>
#include <QBitmap>
#include <QLabel>
#include "rfb/ServerParams.h"
#include "rfb/LogWriter.h"
#include "rdr/Exception.h"
#include "i18n.h"
#include "parameters.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"
#include "msgwriter.h"
#include "vncmacview.h"

#include <QDebug>
#include <QMouseEvent>

#ifdef __APPLE__
#include "cocoa.h"
extern const unsigned short code_map_osx_to_qnum[];
extern const unsigned int code_map_osx_to_qnum_len;
#endif

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
 , m_view(0)
 , m_cursor(nullptr)
{
  setAttribute(Qt::WA_NoBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance()->connection(), &QVNCConnection::framebufferResized, this, [this](int width, int height) {
    PlatformPixelBuffer *framebuffer = (PlatformPixelBuffer*)AppManager::instance()->connection()->framebuffer();
    cocoa_resize(m_view, framebuffer->bitmap());
  }, Qt::QueuedConnection);

  m_overlayTip->hide();
}

QVNCMacView::~QVNCMacView()
{
}

qulonglong QVNCMacView::nativeWindowHandle() const
{
  return (qulonglong)m_view;
}

void QVNCMacView::updateWindow()
{
  QAbstractVNCView::updateWindow();
  draw();
}

bool QVNCMacView::event(QEvent *e)
{
  switch(e->type()) {
  case QEvent::Polish:
    if (!m_view) {
      //qDebug() << "display numbers:  QMACInfo::display()=" <<  QMACInfo::display() << ", XOpenDisplay(NULL)=" << XOpenDisplay(NULL);
      QVNCConnection *cc = AppManager::instance()->connection();
      PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
      CGImage *bitmap = framebuffer->bitmap();
      m_view = cocoa_create_view(this, bitmap);
      // Do not invoke #fromWinId(), otherwise NSView won't be shown.
      // QWindow *w = windowHandle()->fromWinId((WId)m_view);
      setMouseTracking(true);
    }
    break;
  case QEvent::KeyboardLayoutChange:
    break;
  case QEvent::MouseMove:
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease:
  case QEvent::MouseButtonDblClick:
    qDebug() << "QVNCMacView::event: MouseButton event";
    handleMouseButtonEvent((QMouseEvent*)e);
    break;
  case QEvent::Wheel:
    handleMouseWheelEvent((QWheelEvent*)e);
  case QEvent::WindowBlocked:
    //      if (m_hwnd)
    //        EnableWindow(m_hwnd, false);
    break;
  case QEvent::WindowUnblocked:
    //      if (m_hwnd)
    //        EnableWindow(m_hwnd, true);
    break;
  case QEvent::WindowActivate:
    qDebug() << "WindowActivate";
    grabPointer();
    break;
  case QEvent::WindowDeactivate:
    qDebug() << "WindowDeactivate";
    ungrabPointer();
    break;
#if 0 // On macOS, this block is never used.
  case QEvent::Enter:
    qDebug() << "Enter";
    setFocus();
    grabPointer();
    break;
  case QEvent::Leave:
    qDebug() << "Leave";
    clearFocus();
    ungrabPointer();
    break;
#endif
  case QEvent::CursorChange:
    //qDebug() << "CursorChange";
    e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    return true;
//  case QEvent::Paint:
//    //qDebug() << "QEvent::Paint";
//    draw();
//    e->setAccepted(true);
//    return true;
  case QEvent::MetaCall:
    // qDebug() << "QEvent::MetaCall (signal-slot call)";
    break;
  default:
    qDebug() << "Unprocessed Event: " << e->type();
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
  if (m_view) {
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
    if (!m_firstUpdate && ::remoteResize && cc->server()->supportsSetDesktopSize) {
      postRemoteResizeRequest();
    }
    // Some systems require a grab after the window size has been changed.
    // Otherwise they might hold on to displays, resulting in them being unusable.
    maybeGrabKeyboard();
  }
}

void QVNCMacView::paintEvent(QPaintEvent *event)
{
  draw();
}

bool QVNCMacView::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
  if (eventType == "NSEvent") {
    // Special event that means we temporarily lost some input
    if (cocoa_is_mouse_entered(message)) {
      qDebug() << "nativeEvent: mouseEntered";
      setFocus();
      grabPointer();
      return true;
    }
    else if (cocoa_is_mouse_exited(message)) {
      qDebug() << "nativeEvent: mouseExited";
      clearFocus();
      ungrabPointer();
      return true;
    }
    else if (cocoa_is_mouse_moved(message)) {
      int x = 0;
      int y = 0;
      int buttonMask = 0;
      cocoa_get_mouse_properties(message, &x, &y, &buttonMask);
      y = height() - y;
      qDebug() << "nativeEvent: mouseMoved: x=" << x << ",y=" << y << ",buttonMask=0x" << Qt::hex << buttonMask;
      filterPointerEvent(rfb::Point(x, y), buttonMask);
      return true;
    }

    if (cocoa_is_keyboard_sync(message)) {
      while (!m_downKeySym.empty()) {
        handleKeyRelease(m_downKeySym.begin()->first);
      }
      return true;
    }

    if (cocoa_is_keyboard_event(message)) {
      int keyCode = cocoa_event_keycode(message);
      qDebug() << "nativeEvent: keyEvent: keyCode=" << keyCode << ", hexKeyCode=" << Qt::hex << keyCode;
      if ((unsigned)keyCode >= code_map_osx_to_qnum_len) {
        keyCode = 0;
      }
      else {
        keyCode = code_map_osx_to_qnum[keyCode];
      }
      if (cocoa_is_key_press(message)) {
        rdr::U32 keySym = cocoa_event_keysym(message);
        if (keySym == NoSymbol) {
          vlog.error(_("No symbol for key code 0x%02x (in the current state)"), (int)keyCode);
        }

        handleKeyPress(keyCode, keySym);

        // We don't get any release events for CapsLock, so we have to
        // send the release right away.
        if (keySym == XK_Caps_Lock) {
          handleKeyRelease(keyCode);
        }
      }
      else {
        handleKeyRelease(keyCode);
      }

      return true;
    }
  }
  qDebug() << "nativeEvent: eventType=" << eventType;
  return QWidget::nativeEvent(eventType, message, result);
}

void QVNCMacView::bell()
{
  cocoa_beep();
}

void QVNCMacView::draw()
{
  if (!m_view || !AppManager::instance()->view()) {
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
    cocoa_draw(m_view, x, y, w, h);
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
  if (buttons & Qt::MidButton) {
    buttonMask |= 2;
  }
  if (buttons & Qt::RightButton) {
    buttonMask |= 4;
  }

  qDebug() << "handleMouseButtonEvent: x=" << e->x() << ",y=" << e->y();
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
  if (buttons & Qt::MidButton) {
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
}

void QVNCMacView::handleKeyPress(int keyCode, quint32 keySym)
{
  static bool menuRecursion = false;

  // Prevent recursion if the menu wants to send its own
  // activation key.
  if (m_menuKeySym && (keySym == m_menuKeySym) && !menuRecursion) {
    menuRecursion = true;
    popupContextMenu();
    menuRecursion = false;
    return;
  }

  if (viewOnly)
    return;

  if (keyCode == 0) {
    vlog.error(_("No key code specified on key press"));
    return;
  }

  // Because of the way keyboards work, we cannot expect to have the same
  // symbol on release as when pressed. This breaks the VNC protocol however,
  // so we need to keep track of what keysym a key _code_ generated on press
  // and send the same on release.
  m_downKeySym[keyCode] = keySym;

  //  vlog.debug("Key pressed: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(keySym), keySym);

  try {
    QVNCConnection *cc = AppManager::instance()->connection();
    // Fake keycode?
    if (keyCode > 0xff)
      cc->writer()->writeKeyEvent(keySym, 0, true);
    else
      cc->writer()->writeKeyEvent(keySym, keyCode, true);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }
}

void QVNCMacView::handleKeyRelease(int keyCode)
{
  DownMap::iterator iter;

  if (viewOnly)
    return;

  iter = m_downKeySym.find(keyCode);
  if (iter == m_downKeySym.end()) {
    // These occur somewhat frequently so let's not spam them unless
    // logging is turned up.
    vlog.debug("Unexpected release of key code %d", keyCode);
    return;
  }

  //  vlog.debug("Key released: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(iter->second), iter->second);

  try {
    QVNCConnection *cc = AppManager::instance()->connection();
    if (keyCode > 0xff)
      cc->writer()->writeKeyEvent(iter->second, 0, false);
    else
      cc->writer()->writeKeyEvent(iter->second, keyCode, false);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }

  m_downKeySym.erase(iter);
}

void QVNCMacView::setQCursor(const QCursor &cursor)
{
  m_cursor = cocoa_set_cursor(m_view, &cursor);
}

void QVNCMacView::grabKeyboard()
{
  int ret = cocoa_capture_displays(this);
  if (ret != 0) {
    vlog.error(_("Failure grabbing keyboard"));
    return;
  }
  QAbstractVNCView::grabKeyboard();
}

void QVNCMacView::ungrabKeyboard()
{
  QAbstractVNCView::ungrabKeyboard();
}
