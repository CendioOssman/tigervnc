#include <QEvent>
#include <QTextStream>
#include <QDataStream>
#include <QUrl>
#include <QWindow>
#include <QImage>
#include <QBitmap>
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
 , m_region(new rfb::Region)
{
  setAttribute(Qt::WA_NoBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance(), &AppManager::invalidateRequested, this, &QVNCMacView::addInvalidRegion, Qt::QueuedConnection);
  installEventFilter(this);
}

QVNCMacView::~QVNCMacView()
{
  cocoa_delete_cursor(m_cursor);
}

qulonglong QVNCMacView::nativeWindowHandle() const
{
  return (qulonglong)m_view;
}

void QVNCMacView::addInvalidRegion(int x0, int y0, int x1, int y1)
{
  m_region->assign_union(rfb::Rect(x0, y0, x1, y1));

  int w = x1 - x0;
  int h = y1 - y0;
  if (w <= 0 || h <= 0) {
    return;
  }
  draw();
  #if 0
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect r = framebuffer->getDamage();
  //qDebug() << "QQVNCMacView::addInvalidRegion: x=" << r.tl.x << ", y=" << r.tl.y << ", w=" << (r.br.x-r.tl.x) << ", h=" << (r.br.y-r.tl.y);

  // copy the specified region in XImage (== data in framebuffer) to Pixmap.
  Pixmap pixmap = framebuffer->pixmap();
  XGCValues gcvalues;
  GC gc = XCreateGC(display(), pixmap, 0, &gcvalues);
  XImage *xim = framebuffer->ximage();
  XShmSegmentInfo *shminfo = framebuffer->shmSegmentInfo();
  if (shminfo) {
    int ret = XShmPutImage(display(), pixmap, gc, xim, x0, y0, x0, y0, w, h, False);
    //int ret = XShmPutImage(display(), m_window, gc, xim, x0, y0, x0, y0, w, h, False);
    //qDebug() << "XShmPutImage: ret=" << ret;
    // Need to make sure the X server has finished reading the
    // shared memory before we return
    XSync(display(), False);
  } else {
    int ret = XPutImage(display(), pixmap, gc, xim, x0, y0, x0, y0, w, h);
    //qDebug() << "XPutImage(pixmap):ret=" << ret;
  }

  XFreeGC(display(), gc);

  update(x0, y0, w, h);
  #endif
}

void QVNCMacView::updateWindow()
{
  QAbstractVNCView::updateWindow();
  // Nothing more to do, because invalid regions are notified to Qt by addInvalidRegion().
}

/*!
\reimp
*/
bool QVNCMacView::event(QEvent *e)
{
  switch(e->type()) {
  case QEvent::Polish:
    if (!m_view) {
      //qDebug() << "display numbers:  QMACInfo::display()=" <<  QMACInfo::display() << ", XOpenDisplay(NULL)=" << XOpenDisplay(NULL);
      QVNCConnection *cc = AppManager::instance()->connection();
      PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
      NSBitmapImageRep *bitmap = framebuffer->bitmap();
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
    //qDebug() << "WindowActivate";
    grabPointer();
    break;
  case QEvent::WindowDeactivate:
    //qDebug() << "WindowDeactivate";
    ungrabPointer();
    break;
    //    case QEvent::Enter:
    //      qDebug() << "Enter";
    //      grabPointer();
    //      break;
    //    case QEvent::Leave:
    //      qDebug() << "Leave";
    //      ungrabPointer();
    //      break;
  case QEvent::CursorChange:
    //qDebug() << "CursorChange";
    e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    return true;
  case QEvent::Paint:
    //qDebug() << "QEvent::Paint";
    draw();
    e->setAccepted(true);
    return true;
  default:
    qDebug() << "Unprocessed Event: " << e->type();
    break;
  }
  return QWidget::event(e);
}

/*!
\reimp
*/
void QVNCMacView::showEvent(QShowEvent *e)
{
  QWidget::showEvent(e);
}

/*!
\reimp
*/
void QVNCMacView::focusInEvent(QFocusEvent *e)
{
  QWidget::focusInEvent(e);
}

/*!
\reimp
*/
void QVNCMacView::resizeEvent(QResizeEvent *e)
{
  if (m_view) {
    QSize size = e->size();
#if defined(__APPLE__)
    int w = size.width();
    int h = size.height();
#else
    int w = size.width() * m_devicePixelRatio;
    int h = size.height() * m_devicePixelRatio;
#endif
    cocoa_resize(m_view, w, h);

    QWidget::resize(size.width(), size.height());
    //QWidget::resizeEvent(e);
    //adjustSize();

//    bool resizing = (width() != size.width()) || (height() != size.height());
//    if (resizing) {
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
//    }
  }
}

/*!
\reimp
*/
bool QVNCMacView::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
  qDebug() << "nativeEvent: eventType=" << eventType;
  if (eventType == "NSEvent") {
    // Special event that means we temporarily lost some input
    if (cocoa_is_keyboard_sync(message)) {
      while (!m_downKeySym.empty()) {
        handleKeyRelease(m_downKeySym.begin()->first);
      }
      return true;
    }

    if (cocoa_is_keyboard_event(message)) {
      int keyCode = cocoa_event_keycode(message);
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
  rfb::Rect rect = m_region->get_bounding_rect();
  int x0 = rect.tl.x;
  int y0 = rect.tl.y;
  int x1 = rect.br.x;
  int y1 = rect.br.y;
  int w = x1 - x0;
  int h = y1 - y0;
  if (w <= 0 || h <= 0) {
    return;
  }
  //qDebug() << "QVNCMacView::draw: x=" << x0 << ", y=" << y0 << ", w=" << w << ", h=" << h;
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  framebuffer->draw(x0, y0, x0, y0, w, h);

  m_region->clear();
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
  cocoa_delete_cursor(m_cursor);
  m_cursor = cocoa_set_cursor(m_view, &cursor);
#if 0
  #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QBitmap cursorBitmap = cursor.bitmap(Qt::ReturnByValue);
  #else
  QBitmap cursorBitmap = *cursor.bitmap();
  #endif

  QImage image = cursor.pixmap().toImage();
  image.toPixelFormat(QImage::Format_MonoLSB);
  QBitmap maskBitmap = QBitmap::fromImage(image.createHeuristicMask());

  int hotX = cursor.hotSpot().x();
  int hotY = cursor.hotSpot().y();

  int screen = DefaultScreen(display());

  Pixmap cursorPixmap = toPixmap(cursorBitmap);
  Pixmap maskPixmap = toPixmap(maskBitmap);

  XColor color;
  color.pixel = BlackPixel(display(), screen);
  color.red = color.green = color.blue = 0;
  color.flags = DoRed | DoGreen | DoBlue;
  XColor maskColor;
  maskColor.pixel = WhitePixel(display(), screen);
  maskColor.red = maskColor.green = maskColor.blue = 65535;
  maskColor.flags = DoRed | DoGreen | DoBlue;

  Cursor xcursor = XCreatePixmapCursor(display(), cursorPixmap, maskPixmap, &color, &maskColor, hotX, hotY);
  XDefineCursor(display(), nativeWindowHandle(), xcursor);
  XFreeCursor(display(), xcursor);
  XFreeColors(display(), DefaultColormap(display(), screen), &color.pixel, 1, 0);

  XFreePixmap(display(), cursorPixmap);
  XFreePixmap(display(), maskPixmap);
#endif
}

bool QVNCMacView::eventFilter(QObject *obj, QEvent *event)
{
  // standard event processing
  return QObject::eventFilter(obj, event);
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
