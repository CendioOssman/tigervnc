#include <QEvent>
#include <QTextStream>
#include <QDataStream>
#include <QUrl>
#include <QImage>
#include <QBitmap>
#include "rfb/Exception.h"
#include "rfb/ServerParams.h"
#include "rfb/LogWriter.h"
#include "i18n.h"
#include "parameters.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"
#include "msgwriter.h"
#include "touch.h"
#include "vncx11view.h"

#include <X11/Xlib.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif

#include <QDebug>
#include <QMouseEvent>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/XInput2.h>

extern const struct _code_map_xkb_to_qnum {
  const char * from;
  const unsigned short to;
} code_map_xkb_to_qnum[];
extern const unsigned int code_map_xkb_to_qnum_len;

static int code_map_keycode_to_qnum[256];

static rfb::LogWriter vlog("QVNCX11View");

QVNCX11View::QVNCX11View(QWidget *parent, Qt::WindowFlags f)
  : QAbstractVNCView(parent, f)
  , m_window(0)
  , m_region(new rfb::Region)
{
  setAttribute(Qt::WA_NoBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance(), &AppManager::invalidateRequested, this, &QVNCX11View::addInvalidRegion, Qt::QueuedConnection);
  installEventFilter(this);

  XkbDescPtr xkb;
  Status status;

  xkb = XkbGetMap(display(), 0, XkbUseCoreKbd);
  if (!xkb)
    throw rfb::Exception("XkbGetMap");

  status = XkbGetNames(display(), XkbKeyNamesMask, xkb);
  if (status != Success)
    throw rfb::Exception("XkbGetNames");

  memset(code_map_keycode_to_qnum, 0, sizeof(code_map_keycode_to_qnum));
  for (KeyCode keycode = xkb->min_key_code; keycode < xkb->max_key_code; keycode++) {
    const char *keyname = xkb->names->keys[keycode].name;
    if (keyname[0] == '\0')
      continue;

    unsigned short rfbcode = 0;
    for (unsigned i = 0; i < code_map_xkb_to_qnum_len; i++) {
      if (strncmp(code_map_xkb_to_qnum[i].from, keyname, XkbKeyNameLength) == 0) {
        rfbcode = code_map_xkb_to_qnum[i].to;
        break;
      }
    }
    if (rfbcode != 0)
      code_map_keycode_to_qnum[keycode] = rfbcode;
    else {
      code_map_keycode_to_qnum[keycode] = keycode;
      //vlog.debug("No key mapping for key %.4s", keyname);
    }
  }

  XkbFreeKeyboard(xkb, 0, True);
}

QVNCX11View::~QVNCX11View()
{
}

qulonglong QVNCX11View::nativeWindowHandle() const
{
  return (qulonglong)m_window;
}

Display *QVNCX11View::display() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  Display *display = QX11Info::display();
#else
  Display *display = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  return display;
}

void QVNCX11View::addInvalidRegion(int x0, int y0, int x1, int y1)
{
  m_region->assign_union(rfb::Rect(x0, y0, x1, y1));

  int w = x1 - x0;
  int h = y1 - y0;
  if (w <= 0 || h <= 0) {
    return;
  }

  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect r = framebuffer->getDamage();
  //qDebug() << "QQVNCX11View::addInvalidRegion: x=" << r.tl.x << ", y=" << r.tl.y << ", w=" << (r.br.x-r.tl.x) << ", h=" << (r.br.y-r.tl.y);

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
}

void QVNCX11View::updateWindow()
{
  QAbstractVNCView::updateWindow();
  // Nothing more to do, because invalid regions are notified to Qt by addInvalidRegion().
}

/*!
    \reimp
*/
bool QVNCX11View::event(QEvent *e)
{
  switch(e->type()) {
    case QEvent::Polish:
      if (!m_window) {
        //qDebug() << "display numbers:  QX11Info::display()=" <<  QX11Info::display() << ", XOpenDisplay(NULL)=" << XOpenDisplay(NULL);
        int screenNumber = DefaultScreen(display());
        int w = width();
        int h = height();
        int borderWidth = 0;
        QVNCConnection *cc = AppManager::instance()->connection();
        PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
        XSetWindowAttributes xattr;
        xattr.override_redirect = False;
        xattr.background_pixel = 0;
        xattr.border_pixel = 0;
        xattr.colormap = framebuffer->colormap();
        unsigned int wattr = CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap;
        m_window = XCreateWindow(display(), RootWindow(display(), screenNumber), 0, 0, w, h, borderWidth, 32, InputOutput, framebuffer->visualInfo()->visual, wattr, &xattr);
        XReparentWindow(display(), m_window, winId(), 0, 0);
        XMapWindow(display(), m_window);
        setMouseTracking(true);

//        int minKeyCode, maxKeyCode;
//        XDisplayKeycodes(display(), &minKeyCode, &maxKeyCode);
//        int nKeyCodes = maxKeyCode - minKeyCode + 1;
//        int nKeySymsPerKeyCode = 0;
//        KeySym *keySyms = XGetKeyboardMapping(display(), minKeyCode, nKeyCodes, &nKeySymsPerKeyCode);
//        for (int kc = minKeyCode; kc <= maxKeyCode; kc++) {
//          int ix = (kc - minKeyCode) * nKeySymsPerKeyCode;
//          qDebug() << "KeyCode=" << kc << ", KeySym=" << keySyms[ix];
//        }
//        XFree(keySyms);
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
      //qDebug() << "Unprocessed Event: " << e->type();
      break;
  }
  return QWidget::event(e);
}

/*!
    \reimp
*/
void QVNCX11View::showEvent(QShowEvent *e)
{
  QWidget::showEvent(e);
  //XSelectInput(display(), m_window, SubstructureNotifyMask);
}

/*!
    \reimp
*/
void QVNCX11View::focusInEvent(QFocusEvent *e)
{
  QWidget::focusInEvent(e);
}

/*!
    \reimp
*/
void QVNCX11View::resizeEvent(QResizeEvent *e)
{
  if (m_window) {
    QSize size = e->size();
    int w = size.width() * m_devicePixelRatio;
    int h = size.height() * m_devicePixelRatio;
    XResizeWindow(display(), m_window, w, h);
    QWidget::resizeEvent(e);
    adjustSize();

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
bool QVNCX11View::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
  if (eventType == "xcb_generic_event_t") {
    xcb_generic_event_t* ev = static_cast<xcb_generic_event_t *>(message);
    //uint16_t xcbEventType = ev->response_type & ~0x80;
    uint16_t xcbEventType = ev->response_type;
    //qDebug() << "QVNCX11View::nativeEvent: xcbEventType=" << xcbEventType << ",eventType=" << eventType;
    if (xcbEventType == XCB_GE_GENERIC) {
      xcb_ge_generic_event_t* genericEvent = static_cast<xcb_ge_generic_event_t*>(message);
      quint16 geEventType = genericEvent->event_type;
      //qDebug() << "QVNCX11View::nativeEvent: XCB_GE_GENERIC: xcbEventType=" << xcbEventType << ", geEventType=" << geEventType;
      // XI_Inter/XI_Leave causes QEvent::WindowActivate/WindowDeactivate.
//      if (geEventType == XI_Enter) {
//        qDebug() << "######################################################### Enter ##############################################";
//        grabPointer();
//      }
//      else if (geEventType == XI_Leave) {
//        qDebug() << "######################################################### Leave ##############################################";
//        ungrabPointer();
//      }
    }
#if 0
    // seems not working.
    else if (xcbEventType == XCB_BUTTON_PRESS) {
      xcb_button_press_event_t* buttonPressEvent = static_cast<xcb_button_press_event_t*>(message);
      qDebug() << "QVNCX11View::nativeEvent: XCB_BUTTON_PRESS: x=" << buttonPressEvent->root_x << ",y=" << buttonPressEvent->root_y << ",button=" << Qt::hex << buttonPressEvent->detail;
    }
#endif
    else if (xcbEventType == XCB_KEY_PRESS) {
      xcb_key_press_event_t* xevent = reinterpret_cast<xcb_key_press_event_t*>(message);
      //qDebug() << "QVNCX11View::nativeEvent: XCB_KEY_PRESS: keycode=0x" << Qt::hex << xevent->detail << ", state=0x" << xevent->state << ", mapped_keycode=0x" << code_map_keycode_to_qnum[xevent->detail];

      int keycode = code_map_keycode_to_qnum[xevent->detail]; // TODO: what's this table???
      //int keycode = xevent->detail;
      if (keycode == 50) {
        keycode = 42;
      }

      // Generate a fake keycode just for tracking if we can't figure
      // out the proper one
      if (keycode == 0)
        keycode = 0x100 | xevent->detail;

      XKeyEvent kev;
      kev.type = xevent->response_type;
      kev.serial = xevent->sequence;
      kev.send_event = false;
      kev.display = display();
      kev.window = xevent->event;
      kev.root = xevent->root;
      kev.subwindow = xevent->child;
      kev.time = xevent->time;
      kev.x = xevent->event_x;
      kev.y = xevent->event_y;
      kev.x_root = xevent->root_x;
      kev.y_root = xevent->root_y;
      kev.state = xevent->state;
      kev.keycode = xevent->detail;
      kev.same_screen = xevent->same_screen;
      char buffer[10];
      KeySym keysym;
      XLookupString(&kev, buffer, sizeof(buffer), &keysym, NULL);

      if (keysym == NoSymbol) {
        vlog.error(_("No symbol for key code %d (in the current state)"), (int)xevent->detail);
      }

      switch (keysym) {
        // For the first few years, there wasn't a good consensus on what the
        // Windows keys should be mapped to for X11. So we need to help out a
        // bit and map all variants to the same key...
        case XK_Hyper_L:
          keysym = XK_Super_L;
          break;
        case XK_Hyper_R:
          keysym = XK_Super_R;
          break;
          // There has been several variants for Shift-Tab over the years.
          // RFB states that we should always send a normal tab.
        case XK_ISO_Left_Tab:
          keysym = XK_Tab;
          break;
      }

      handleKeyPress(keycode, keysym);
      return true;
    }
    else if (xcbEventType == XCB_KEY_RELEASE) {
      xcb_key_release_event_t* xevent = reinterpret_cast<xcb_key_release_event_t*>(message);
      int keycode = code_map_keycode_to_qnum[xevent->detail]; // TODO: what's this table???
      //int keycode = xevent->detail;
      if (keycode == 0)
        keycode = 0x100 | xevent->detail;
      handleKeyRelease(keycode);
      return true;
    }
    else if (xcbEventType == XCB_EXPOSE) {

    }
    else if (xcbEventType == XCB_ENTER_NOTIFY) {
      // Won't reach here, because Enter/Leave events are handled by XInput.
      //qDebug() << "XCB_ENTER_NOTIFY";
      grabPointer();
    }
    else if (xcbEventType == XCB_LEAVE_NOTIFY) {
      // Won't reach here, because Enter/Leave events are handled by XInput.
      //qDebug() << "XCB_LEAVE_NOTIFY";
      ungrabPointer();
    }
  }
  //return false;
  return QWidget::nativeEvent(eventType, message, result);
}

void QVNCX11View::bell()
{
  XBell(display(), 0 /* volume */);
}

void QVNCX11View::draw()
{
  if (!m_window || !AppManager::instance()->view()) {
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
  //qDebug() << "QVNCX11View::draw: x=" << x0 << ", y=" << y0 << ", w=" << w << ", h=" << h;
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  framebuffer->draw(x0, y0, x0, y0, w, h);

  m_region->clear();
}

// Viewport::handle(int event)
void QVNCX11View::handleMouseButtonEvent(QMouseEvent *e)
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
void QVNCX11View::handleMouseWheelEvent(QWheelEvent *e)
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

void QVNCX11View::handleKeyPress(int keyCode, quint32 keySym)
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

  vlog.debug("Key pressed: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(keySym), keySym);

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

void QVNCX11View::handleKeyRelease(int keyCode)
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

  vlog.debug("Key released: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(iter->second), iter->second);

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

Pixmap QVNCX11View::toPixmap(QBitmap &bitmap)
{
  QImage image = bitmap.toImage();

  int xbytes = (image.width() + 7) / 8;
  int ybytes = image.height();
  char *data = new char[xbytes * ybytes];
  uchar *src = image.bits();
  char *dst = data;
  for (int y = 0; y < ybytes; y++) {
    memcpy(dst, src, xbytes);
    src += image.bytesPerLine();
    dst += xbytes;
  }

  Pixmap pixmap = XCreateBitmapFromData(display(), nativeWindowHandle(), data, image.width(), image.height());

  delete[] data;
  return pixmap;
}

void QVNCX11View::setQCursor(const QCursor &cursor)
{
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
}

void QVNCX11View::grabPointer()
{
  //x11_grab_pointer(nativeWindowHandle());
  setMouseTracking(true);
  m_mouseGrabbed = true;
}

void QVNCX11View::ungrabPointer()
{
  //x11_ungrab_pointer(nativeWindowHandle());
  setMouseTracking(false);
  m_mouseGrabbed = false;
}

bool QVNCX11View::eventFilter(QObject *obj, QEvent *event)
{
//  if (event->type() == QEvent::MouseMove) {
//    QMouseEvent *e = static_cast<QMouseEvent *>(event);
//    qDebug() << "QVNCX11View::eventFilter: pos=" << e->pos();
//    return false;
//  }
//  else if (event->type() == QEvent::Enter) {
//    qDebug() << "QVNCX11View::eventFilter: event type=Enter";
//    return false;
//  }
//  else if (event->type() == QEvent::Leave) {
//    qDebug() << "QVNCX11View::eventFilter: event type=Leave";
//    return false;
//  }

  // standard event processing
  return QObject::eventFilter(obj, event);
}
