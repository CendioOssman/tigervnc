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
  , m_display(nullptr)
  , m_screen(0)
  , m_visualInfo(nullptr)
  , m_colorMap(0)
  , m_pixmap(0)
  , m_picture(0)
{
  setAttribute(Qt::WA_NoBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance()->connection(), &QVNCConnection::framebufferResized, this, &QVNCX11View::resizePixmap, Qt::QueuedConnection);
  installEventFilter(this);

#if 1
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  m_display = QX11Info::display();
#else
  m_display = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()->m_display;
#endif
  m_screen = DefaultScreen(m_display);
  XVisualInfo vtemplate;
  int nvinfo;
  XVisualInfo *visualList = XGetVisualInfo(m_display, 0, &vtemplate, &nvinfo);
  XVisualInfo *found = 0;
  for (int i = 0; i < nvinfo; i++) {
    if (visualList[i].c_class == StaticColor || visualList[i].c_class == TrueColor) {
      if (!found || found->depth < visualList[i].depth) {
        found = &visualList[i];
      }
    }
  }
  m_visualInfo = found;
  m_colorMap = XCreateColormap(m_display, RootWindow(m_display, m_screen), m_visualInfo->visual, AllocNone);
  m_visualFormat = XRenderFindVisualFormat(m_display, m_visualInfo->visual);
#endif

  XkbDescPtr xkb;
  Status status;

  xkb = XkbGetMap(m_display, 0, XkbUseCoreKbd);
  if (!xkb)
    throw rfb::Exception("XkbGetMap");

  status = XkbGetNames(m_display, XkbKeyNamesMask, xkb);
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
  if (m_picture) {
    XRenderFreePicture(m_display, m_picture);
  }
  if (m_pixmap) {
    XFreePixmap(m_display, m_pixmap);
  }
}

qulonglong QVNCX11View::nativeWindowHandle() const
{
  return (qulonglong)m_window;
}

void QVNCX11View::resizePixmap(int width, int height)
{
  if (m_picture) {
    XRenderFreePicture(m_display, m_picture);
  }
  if (m_pixmap) {
    XFreePixmap(m_display, m_pixmap);
  }
  m_pixmap = XCreatePixmap(m_display, RootWindow(m_display, m_screen), width, height, 32);
  qDebug() << "Surface::alloc: XCreatePixmap: w=" << width << ", h=" << height << ", pixmap=" << m_pixmap;

  // Our code assumes a BGRA byte order, regardless of what the endian
  // of the machine is or the native byte order of XImage, so make sure
  // we find such a format
  XRenderPictFormat templ;
  templ.type = PictTypeDirect;
  templ.depth = 32;
  if (XImageByteOrder(m_display) == MSBFirst) {
    templ.direct.alpha = 0;
    templ.direct.red   = 8;
    templ.direct.green = 16;
    templ.direct.blue  = 24;
  }
  else {
    templ.direct.alpha = 24;
    templ.direct.red   = 16;
    templ.direct.green = 8;
    templ.direct.blue  = 0;
  }
  templ.direct.alphaMask = 0xff;
  templ.direct.redMask = 0xff;
  templ.direct.greenMask = 0xff;
  templ.direct.blueMask = 0xff;

  XRenderPictFormat *format = XRenderFindFormat(m_display, PictFormatType | PictFormatDepth |
                                                PictFormatRed | PictFormatRedMask |
                                                PictFormatGreen | PictFormatGreenMask |
                                                PictFormatBlue | PictFormatBlueMask |
                                                PictFormatAlpha | PictFormatAlphaMask,
                                                &templ, 0);
  if (!format) {
    throw rdr::Exception("XRenderFindFormat");
  }
  m_picture = XRenderCreatePicture(m_display, m_pixmap, format, 0, NULL);
}

void QVNCX11View::updateWindow()
{
  QAbstractVNCView::updateWindow();
  draw();
}

/*!
    \reimp
*/
bool QVNCX11View::event(QEvent *e)
{
  switch(e->type()) {
    case QEvent::Polish:
      if (!m_window) {
        int screenNumber = DefaultScreen(m_display);
        int w = width();
        int h = height();
        int borderWidth = 0;
        XSetWindowAttributes xattr;
        xattr.override_redirect = False;
        xattr.background_pixel = 0;
        xattr.border_pixel = 0;
        xattr.colormap = m_colorMap;
        unsigned int wattr = CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap;
        m_window = XCreateWindow(m_display, RootWindow(m_display, screenNumber), 0, 0, w, h, borderWidth, 32, InputOutput, m_visualInfo->visual, wattr, &xattr);
        XReparentWindow(m_display, m_window, winId(), 0, 0);
        XMapWindow(m_display, m_window);
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
  //XSelectInput(m_display, m_window, SubstructureNotifyMask);
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
    XResizeWindow(m_display, m_window, w, h);
    QWidget::resizeEvent(e);
    adjustSize();

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
      kev.display = m_display;
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
  XBell(m_display, 0 /* volume */);
}

void QVNCX11View::draw()
{
  if (!m_window || !AppManager::instance()->view()) {
    return;
  }
  #if 0
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
  //QVNCConnection *cc = AppManager::instance()->connection();
  //PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  //framebuffer->draw(x0, y0, x0, y0, w, h);
//  Picture picture = framebuffer->picture();
  Picture winPict = XRenderCreatePicture(m_display, m_window, m_visualFormat, 0, NULL);
  XRenderComposite(m_display, PictOpSrc, m_picture, None, winPict, x0, y0, 0, 0, x0, y0, w, h);
  XRenderFreePicture(m_display, winPict);

  m_region->clear();
  #else
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect rect = framebuffer->getDamage();
  int x = rect.tl.x;
  int y = rect.tl.y;
  int w = rect.br.x - x;
  int h = rect.br.y - y;
  if (!rect.is_empty()) {
    qDebug() << "QVNCX11View::draw: x=" << x << ", y=" << y << ", w=" << w << ", h=" << h;
    // copy the specified region in XImage (== data in framebuffer) to Pixmap.
    XGCValues gcvalues;
    GC gc = XCreateGC(m_display, m_pixmap, 0, &gcvalues);
    XImage *xim = framebuffer->ximage();
    XShmSegmentInfo *shminfo = framebuffer->shmSegmentInfo();
    if (shminfo) {
      XShmPutImage(m_display, m_pixmap, gc, xim, x, y, x, y, w, h, False);
      // Need to make sure the X server has finished reading the
      // shared memory before we return
      XSync(m_display, False);
    }
    else {
      XPutImage(m_display, m_pixmap, gc, xim, x, y, x, y, w, h);
    }

    XFreeGC(m_display, gc);

  
    Picture winPict = XRenderCreatePicture(m_display, m_window, m_visualFormat, 0, NULL);
    XRenderComposite(m_display, PictOpSrc, m_picture, None, winPict, x, y, 0, 0, x, y, w, h);
    XRenderFreePicture(m_display, winPict);
  }
  #endif
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

  Pixmap pixmap = XCreateBitmapFromData(m_display, nativeWindowHandle(), data, image.width(), image.height());

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

  int screen = DefaultScreen(m_display);

  Pixmap cursorPixmap = toPixmap(cursorBitmap);
  Pixmap maskPixmap = toPixmap(maskBitmap);

  XColor color;
  color.pixel = BlackPixel(m_display, screen);
  color.red = color.green = color.blue = 0;
  color.flags = DoRed | DoGreen | DoBlue;
  XColor maskColor;
  maskColor.pixel = WhitePixel(m_display, screen);
  maskColor.red = maskColor.green = maskColor.blue = 65535;
  maskColor.flags = DoRed | DoGreen | DoBlue;

  Cursor xcursor = XCreatePixmapCursor(m_display, cursorPixmap, maskPixmap, &color, &maskColor, hotX, hotY);
  XDefineCursor(m_display, nativeWindowHandle(), xcursor);
  XFreeCursor(m_display, xcursor);
  XFreeColors(m_display, DefaultColormap(m_display, screen), &color.pixel, 1, 0);

  XFreePixmap(m_display, cursorPixmap);
  XFreePixmap(m_display, maskPixmap);
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

void QVNCX11View::grabKeyboard()
{
#if 1
  XGrabKeyboard(m_display, nativeWindowHandle(), True, GrabModeAsync, GrabModeAsync, CurrentTime);
#else
  unsigned long serial = XNextRequest(m_display);

  int ret = XGrabKeyboard(m_display, nativeWindowHandle(), True, GrabModeAsync, GrabModeAsync, CurrentTime);
  if (ret) {
    if (ret == AlreadyGrabbed) {
      // It seems like we can race with the WM in some cases.
      // Try again in a bit.
      if (!Fl::has_timeout(handleGrab, this))
        Fl::add_timeout(0.500, handleGrab, this);
    } else {
      vlog.error(_("Failure grabbing keyboard"));
    }
    return;
  }

  // Xorg 1.20+ generates FocusIn/FocusOut even when there is no actual
  // change of focus. This causes us to get stuck in an endless loop
  // grabbing and ungrabbing the keyboard. Avoid this by filtering out
  // any focus events generated by XGrabKeyboard().
  XSync(m_display, False);
  XEvent xev;
  while (XCheckIfEvent(m_display, &xev, &eventIsFocusWithSerial, (XPointer)&serial) == True) {
    vlog.debug("Ignored synthetic focus event cause by grab change");
  }
#endif

  QAbstractVNCView::grabKeyboard();
}

void QVNCX11View::ungrabKeyboard()
{
#if 1
  XUngrabKeyboard(m_display, CurrentTime);
#else
  XEvent xev;
  unsigned long serial = XNextRequest(m_display);

  XUngrabKeyboard(m_display, CurrentTime);

  // See grabKeyboard()
  XSync(m_display, False);
  while (XCheckIfEvent(m_display, &xev, &eventIsFocusWithSerial, (XPointer)&serial) == True) {
    vlog.debug("Ignored synthetic focus event cause by grab change");
  }
#endif
  
  QAbstractVNCView::ungrabKeyboard();
}

void QVNCX11View::grabPointer()
{
  //x11_grab_pointer(nativeWindowHandle());
  QAbstractVNCView::grabPointer();
}

void QVNCX11View::ungrabPointer()
{
  //x11_ungrab_pointer(nativeWindowHandle());
  QAbstractVNCView::ungrabPointer();
}
