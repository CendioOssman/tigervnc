#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QCursor>
#include <qt_windows.h>

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"
#include "rfb/LogWriter.h"
#include "rfb/ledStates.h"
#include "rfb/ServerParams.h"
#include "rdr/Exception.h"

#include "appmanager.h"
#include "parameters.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"
#include "win32.h"
#include "i18n.h"
#include "viewerconfig.h"
#include "vncwinview.h"

#include <QDebug>
#include <QMessageBox>
#include <QTime>

static rfb::LogWriter vlog("Viewport");

// Used to detect fake input (0xaa is not a real key)
static const WORD SCAN_FAKE = 0xaa;
static const WORD NoSymbol = 0;

QVNCWinView::QVNCWinView(QWidget *parent, Qt::WindowFlags f)
 : QAbstractVNCView(parent, f)
 , m_wndproc(0)
 , m_hwndowner(false)
 , m_hwnd(0)
 , m_altGrArmed(false)
 , m_altGrCtrlTimer(new QTimer)
 , m_cursor(nullptr)
 , m_mouseTracking(false)
 , m_defaultCursor(LoadCursor(NULL, IDC_ARROW))
{
  setAttribute(Qt::WA_NoBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_InputMethodTransparent);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance()->connection(), &QVNCConnection::framebufferResized, this, [this](int width, int height) {
    SetWindowPos(m_hwnd, HWND_TOP, 0, 0, width, height, 0);
  }, Qt::QueuedConnection);

  m_altGrCtrlTimer->setInterval(100);
  m_altGrCtrlTimer->setSingleShot(true);
  connect(m_altGrCtrlTimer, &QTimer::timeout, this, [this]() {
    m_altGrArmed = false;
    handleKeyPress(0x1d, XK_Control_L);
  });
}

QVNCWinView::~QVNCWinView()
{
  if (m_wndproc) {
    SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_wndproc);
  }

  if (m_hwnd && m_hwndowner) {
    DestroyWindow(m_hwnd);
  }

  m_altGrCtrlTimer->stop();
  delete m_altGrCtrlTimer;

  DestroyIcon(m_cursor);
}

qulonglong QVNCWinView::nativeWindowHandle() const
{
  return (qulonglong)m_hwnd;
}

HWND QVNCWinView::createWindow(HWND parent, HINSTANCE instance)
{
  static ATOM windowClass = 0;
  if (!windowClass) {
    WNDCLASSEX wcex;
    wcex.cbSize		= sizeof(WNDCLASSEX);
    wcex.style		= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= (WNDPROC)eventHandler;
    wcex.cbClsExtra	= 0;
    wcex.cbWndExtra	= 0;
    wcex.hInstance	= instance;
    wcex.hIcon		= NULL;
    wcex.hCursor	= NULL;
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= "VNC Window";
    wcex.hIconSm	= NULL;

    windowClass = RegisterClassEx(&wcex);
  }

  HWND hwnd = CreateWindow("VNC Window", 0, WS_CHILD|WS_CLIPSIBLINGS|WS_TABSTOP, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, parent, NULL, instance, NULL);
  return hwnd;
}

void QVNCWinView::fixParent()
{
  if (!m_hwnd) {
    return;
  }
  if (!::IsWindow(m_hwnd)) {
    m_hwnd = 0;
    return;
  }
  if (::GetParent(m_hwnd) == (HWND)winId()) {
    return;
  }
  long style = GetWindowLong(m_hwnd, GWL_STYLE);
  if (style & WS_OVERLAPPED) {
    return;
  }
  ::SetParent(m_hwnd, (HWND)winId());
}

void QVNCWinView::setWindow(HWND window)
{
  if (m_hwnd && m_hwndowner) {
    DestroyWindow(m_hwnd);
  }
  m_hwnd = window;
  fixParent();

  m_hwndowner = false;
}

void QVNCWinView::postMouseMoveEvent(int x, int y, int mask)
{
  if (::viewOnly) {
    return;
  }
  rfb::Point p(x, y);
  emit AppManager::instance()->connection()->writePointerEvent(p, mask);
}

void *getWindowProc(QVNCWinView *host)
{
  return host ? host->m_wndproc : 0;
}

LRESULT CALLBACK QVNCWinView::eventHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  QWidget *widget = QWidget::find((WId)GetParent(hWnd));
  QVNCWinView *window = qobject_cast<QVNCWinView*>(widget);

  if (window) {
    switch (message) {
    case WM_MOUSEMOVE: {
      window->startMouseTracking();
      int x, y, buttonMask, wheelMask;
      getMouseProperties(wParam, lParam, x, y, buttonMask, wheelMask);
      window->postMouseMoveEvent(x, y, buttonMask | wheelMask);
      //qDebug() << "VNCWinView::eventHandler(): WM_MOUSEMOVE: x=" << x << ", y=" << y;
    }
      break;
    case WM_MOUSELEAVE:
      window->stopMouseTracking();
      break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_XBUTTONUP:
    case WM_XBUTTONDOWN: {
      if (::viewOnly) {
        break;
      }
      int x, y, buttonMask, wheelMask;
      getMouseProperties(wParam, lParam, x, y, buttonMask, wheelMask);
      rfb::Point p(x, y);
      emit AppManager::instance()->connection()->writePointerEvent(p, buttonMask | wheelMask);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
      qDebug() << "QVNCWinView::eventHandler (button up/down): x=" << x << ", y=" << y << ", btn=" << Qt::hex << (buttonMask | wheelMask);
#endif
      if (message == WM_LBUTTONUP || message == WM_MBUTTONUP || message == WM_RBUTTONUP || message == WM_XBUTTONUP) {
	// We usually fail to grab the mouse if a mouse button was
	// pressed when we gained focus (e.g. clicking on our window),
	// so we may need to try again when the button is released.
	// (We do it here rather than handle() because a window does not
	// see FL_RELEASE events if a child widget grabs it first)
	if (window->m_keyboardGrabbed && !window->m_mouseGrabbed) {
	  window->grabPointer();
	}
      }
    }
      break;
    case WM_SETFOCUS:
      qDebug() << "VNCWinView::eventHandler(): WM_SETFOCUS";
      window->maybeGrabKeyboard();
      break;
    case WM_KILLFOCUS:
      qDebug() << "VNCWinView::eventHandler(): WM_KILLFOCUS";
      if (fullscreenSystemKeys) {
        window->ungrabKeyboard();
      }
      break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      window->handleKeyDownEvent(message, wParam, lParam);
      break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      window->handleKeyUpEvent(message, wParam, lParam);
      break;
    case WM_PAINT:
      AppManager::instance()->connection()->refreshFramebuffer();
      AppManager::instance()->view()->updateWindow();
      break;
    case WM_WINDOWPOSCHANGED: {
      // Use WM_WINDOWPOSCHANGED instead of WM_SIZE. WM_SIZE is being sent while the window size is changing, whereas
      // WM_WINDOWPOSCHANGED is sent only a couple of times when the window sizing completes.
      LPWINDOWPOS pos = (LPWINDOWPOS)lParam;
      int lw = pos->cx;
      int lh = pos->cy;
      qDebug() << "VNCWinView::eventHandler(): WM_WINDOWPOSCHANGED: w=" << lw << ", h=" << lh << "current w=" << AppManager::instance()->view()->width() << "current h=" << AppManager::instance()->view()->height() << "devicePixelRatio=" << AppManager::instance()->view()->devicePixelRatio();
      // Do not rely on the lParam's geometry, because it's sometimes incorrect, especially when the fullscreen is activated.
      // See the following URL for more information.
      // https://stackoverflow.com/questions/52157587/why-qresizeevent-qwidgetsize-gives-different-when-fullscreen
      int w = AppManager::instance()->view()->width() * AppManager::instance()->view()->devicePixelRatio() + 0.5;
      int h = AppManager::instance()->view()->height() * AppManager::instance()->view()->devicePixelRatio() + 0.5;
      SetWindowPos(hWnd, HWND_TOP, 0, 0, w, h, 0);
      qDebug() << "VNCWinView::eventHandler(): SetWindowPos: w=" << w << ", h=" << h;
    }
      break;
    default:
      qDebug() << "VNCWinView::eventHandler() (DefWindowProc): message=" << message;
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
  }
  return 0;
}

void QVNCWinView::getMouseProperties(WPARAM wParam, LPARAM lParam, int &x, int &y, int &buttonMask, int &wheelMask)
{
  short h = (short)HIWORD(wParam);
  WORD l = LOWORD(wParam);
  buttonMask = 0;
  wheelMask = 0;
  if (l & MK_LBUTTON) {
    buttonMask |= 1;
  }
  if (l & MK_MBUTTON) {
    buttonMask |= 2;
  }
  if (l & MK_RBUTTON) {
    buttonMask |= 4;
  }
  if (l & MK_XBUTTON1) {
    wheelMask |= 32;
  }
  if (l & MK_XBUTTON2) {
    wheelMask |= 64;
  }
  if (h < 0) {
    wheelMask |= 8;
  }
  if (h > 0) {
    wheelMask |= 16;
  }

  x = (lParam & 0x0000ffff);
  y = ((lParam & 0xffff0000) >> 16);
}

void QVNCWinView::refresh(HWND hWnd)
{
  qDebug() << "VNCWinView::refresh(): hWnd=" << hWnd;
  PlatformPixelBuffer *framebuffer = (PlatformPixelBuffer *)AppManager::instance()->connection()->framebuffer();
  rfb::Rect r = framebuffer->getDamage();
  int x = r.tl.x;
  int y = r.tl.y;
  int width = r.br.x - x;
  int height = r.br.y - y;
  RECT rect{x, y, x + width, y + height};
  InvalidateRect(m_hwnd, &rect, false);

  PAINTSTRUCT ps;
  HDC hDC = BeginPaint(hWnd, &ps);
  HBITMAP hBitmap = framebuffer->hbitmap();
  BITMAP bitmap;
  GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bitmap);
  HDC hDCBits = CreateCompatibleDC(hDC);
  SelectObject(hDCBits, hBitmap);
  BitBlt(hDC, x, y, width, height, hDCBits, x, y, SRCCOPY);
  DeleteDC(hDCBits);
  EndPaint(hWnd, &ps);
}

bool QVNCWinView::event(QEvent *e)
{
  switch(e->type()) {
  case QEvent::Polish:
    if (!m_hwnd) {
      m_hwnd = createWindow(HWND(winId()), GetModuleHandle(0));
      fixParent();
      m_hwndowner = m_hwnd != 0;
    }
    if (m_hwnd && !m_wndproc && GetParent(m_hwnd) == (HWND)winId()) {
      m_wndproc = (void*)GetWindowLongPtr(m_hwnd, GWLP_WNDPROC);
      SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)eventHandler);

      LONG style;
      style = GetWindowLong(m_hwnd, GWL_STYLE);
      if (style & WS_TABSTOP) {
        setFocusPolicy(Qt::FocusPolicy(focusPolicy() | Qt::StrongFocus));
      }
    }
    break;
  case QEvent::WindowBlocked:
    if (m_hwnd)
      EnableWindow(m_hwnd, false);
    break;
  case QEvent::WindowUnblocked:
    if (m_hwnd)
      EnableWindow(m_hwnd, true);
    break;
  case QEvent::WindowActivate:
    //qDebug() << "WindowActivate";
    break;
  case QEvent::WindowDeactivate:
    //qDebug() << "WindowDeactivate";
    break;
  case QEvent::Enter:
    //qDebug() << "Enter";
    grabPointer();
    break;
  case QEvent::Leave:
    //qDebug() << "Leave";
    ungrabPointer();
    break;
  case QEvent::CursorChange:
    //qDebug() << "CursorChange";
    e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    break;
  case QEvent::Paint:
    //qDebug() << "Paint";
    e->setAccepted(true);
    return true;
  default:
    qDebug() << "Unprocessed Event: " << e->type();
    break;
  }
  return QWidget::event(e);
}

void QVNCWinView::showEvent(QShowEvent *e)
{
  QWidget::showEvent(e);

  if (m_hwnd) {
    int w = width() * m_devicePixelRatio;
    int h = height() * m_devicePixelRatio;
    SetWindowPos(m_hwnd, HWND_TOP, 0, 0, w, h, SWP_SHOWWINDOW);
    qDebug() << "VNCWinView::showEvent(): SetWindowPos: w=" << w << ", h=" << h;
  }
}

void QVNCWinView::focusInEvent(QFocusEvent *e)
{
  QWidget::focusInEvent(e);

  if (m_hwnd) {
    ::SetFocus(m_hwnd);
  }
}

void QVNCWinView::resizeEvent(QResizeEvent *e)
{
  qDebug() << "QVNCWinView::resizeEvent: w=" << e->size().width() << ", h=" << e->size().height();

  if (m_hwnd) {
    // Try to get the remote size to match our window size, provided
    // the following conditions are true:
    //
    // a) The user has this feature turned on
    // b) The server supports it
    // c) We're not still waiting for startup fullscreen to kick in
    //
    QVNCConnection *cc = AppManager::instance()->connection();
    if (!m_firstUpdate && ::remoteResize && cc->server()->supportsSetDesktopSize && !m_fullscreenEnabled && !m_pendingFullscreen) {
      postRemoteResizeRequest();
    }
    // Some systems require a grab after the window size has been changed.
    // Otherwise they might hold on to displays, resulting in them being unusable.
    maybeGrabKeyboard();
  }
}

bool QVNCWinView::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
  MSG *msg = (MSG *)message;
  switch (msg->message) {
    case WM_SETFOCUS:
      if (m_hwnd) {
        ::SetFocus(m_hwnd);
        return true;
      }
      break;
    default:
      break;
  }
  return QWidget::nativeEvent(eventType, message, result);
}

void QVNCWinView::resolveAltGrDetection(bool isAltGrSequence)
{
  m_altGrArmed = false;
  m_altGrCtrlTimer->stop();
  // when it's not an AltGr sequence we can't supress the Ctrl anymore
  if (!isAltGrSequence)
    handleKeyPress(0x1d, XK_Control_L);
}

void QVNCWinView::handleKeyPress(int keyCode, quint32 keySym)
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

  if (::viewOnly)
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

  vlog.debug("Key pressed: 0x%04x => 0x%04x", keyCode, keySym);

  try {
    // Fake keycode?
    if (keyCode > 0xff)
      emit AppManager::instance()->connection()->writeKeyEvent(keySym, 0, true);
    else
      emit AppManager::instance()->connection()->writeKeyEvent(keySym, keyCode, true);
  }
  catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }
}

void QVNCWinView::handleKeyRelease(int keyCode)
{
  DownMap::iterator iter;

  if (::viewOnly)
    return;

  iter = m_downKeySym.find(keyCode);
  if (iter == m_downKeySym.end()) {
    // These occur somewhat frequently so let's not spam them unless
    // logging is turned up.
    vlog.debug("Unexpected release of key code %d", keyCode);
    return;
  }

  vlog.debug("Key released: 0x%04x => 0x%04x", keyCode, iter->second);

  try {
    if (keyCode > 0xff)
      emit AppManager::instance()->connection()->writeKeyEvent(iter->second, 0, false);
    else
      emit AppManager::instance()->connection()->writeKeyEvent(iter->second, keyCode, false);

  }
  catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }

  m_downKeySym.erase(iter);
}

int QVNCWinView::handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
  Q_UNUSED(message);
  unsigned int timestamp = GetMessageTime();
  UINT vKey = wParam;
  bool isExtended = (lParam & (1 << 24)) != 0;
  int keyCode = ((lParam >> 16) & 0xff);

  // Windows' touch keyboard doesn't set a scan code for the Alt
  // portion of the AltGr sequence, so we need to help it out
  if (!isExtended && (keyCode == 0x00) && (vKey == VK_MENU)) {
    isExtended = true;
    keyCode = 0x38;
  }

  // Windows doesn't have a proper AltGr, but handles it using fake
  // Ctrl+Alt. However the remote end might not be Windows, so we need
  // to merge those in to a single AltGr event. We detect this case
  // by seeing the two key events directly after each other with a very
  // short time between them (<50ms) and supress the Ctrl event.
  if (m_altGrArmed) {
    bool altPressed = isExtended &&
        (keyCode == 0x38) &&
        (vKey == VK_MENU) &&
        ((timestamp - m_altGrCtrlTime) < 50);
    resolveAltGrDetection(altPressed);
  }

  if (keyCode == SCAN_FAKE) {
    vlog.debug("Ignoring fake key press (virtual key 0x%02x)", vKey);
    return 1;
  }

  // Windows sets the scan code to 0x00 for multimedia keys, so we
  // have to do a reverse lookup based on the vKey.
  if (keyCode == 0x00) {
    keyCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
    if (keyCode == 0x00) {
      if (isExtended) {
        vlog.error(_("No scan code for extended virtual key 0x%02x"), (int)vKey);
      }
      else {
        vlog.error(_("No scan code for virtual key 0x%02x"), (int)vKey);
      }
      return 1;
    }
  }

  if (keyCode & ~0x7f) {
    vlog.error(_("Invalid scan code 0x%02x"), (int)keyCode);
    return 1;
  }

  if (isExtended) {
    keyCode |= 0x80;
  }

  // Fortunately RFB and Windows use the same scan code set (mostly),
  // so there is no conversion needed
  // (as long as we encode the extended keys with the high bit)

  // However Pause sends a code that conflicts with NumLock, so use
  // the code most RFB implementations use (part of the sequence for
  // Ctrl+Pause, i.e. Break)
  if (keyCode == 0x45) {
    keyCode = 0xc6;
  }
  // And NumLock incorrectly has the extended bit set
  if (keyCode == 0xc5) {
    keyCode = 0x45;
  }
  // And Alt+PrintScreen (i.e. SysRq) sends a different code than
  // PrintScreen
  if (keyCode == 0xb7) {
    keyCode = 0x54;
  }
  quint32 keySym = win32_vkey_to_keysym(vKey, isExtended);
  if (keySym == NoSymbol) {
    if (isExtended) {
      vlog.error(_("No symbol for extended virtual key 0x%02x"), (int)vKey);
    }
    else {
      vlog.error(_("No symbol for virtual key 0x%02x"), (int)vKey);
    }
  }

  // Windows sends the same vKey for both shifts, so we need to look
  // at the scan code to tell them apart
  if ((keySym == XK_Shift_L) && (keyCode == 0x36)) {
    keySym = XK_Shift_R;
  }
  // AltGr handling (see above)
  if (win32_has_altgr()) {
    if ((keyCode == 0xb8) && (keySym == XK_Alt_R)) {
      keySym = XK_ISO_Level3_Shift;
    }
    // Possible start of AltGr sequence?
    if ((keyCode == 0x1d) && (keySym == XK_Control_L)) {
      m_altGrArmed = true;
      m_altGrCtrlTime = timestamp;
      m_altGrCtrlTimer->start();
      return 1;
    }
  }

  handleKeyPress(keyCode, keySym);

  // We don't get reliable WM_KEYUP for these
  switch (keySym) {
  case XK_Zenkaku_Hankaku:
  case XK_Eisu_toggle:
  case XK_Katakana:
  case XK_Hiragana:
  case XK_Romaji:
    handleKeyRelease(keyCode);
  }

  return 1;
}

int QVNCWinView::handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
  Q_UNUSED(message);
  UINT vKey = wParam;
  bool isExtended = (lParam & (1 << 24)) != 0;
  int keyCode = ((lParam >> 16) & 0xff);

  // Touch keyboard AltGr (see above)
  if (!isExtended && (keyCode == 0x00) && (vKey == VK_MENU)) {
    isExtended = true;
    keyCode = 0x38;
  }

  // We can't get a release in the middle of an AltGr sequence, so
  // abort that detection
  if (m_altGrArmed) {
    resolveAltGrDetection(false);
  }
  if (keyCode == SCAN_FAKE) {
    vlog.debug("Ignoring fake key release (virtual key 0x%02x)", vKey);
    return 1;
  }

  if (keyCode == 0x00) {
    keyCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
  }
  if (isExtended) {
    keyCode |= 0x80;
  }
  if (keyCode == 0x45) {
    keyCode = 0xc6;
  }
  if (keyCode == 0xc5) {
    keyCode = 0x45;
  }
  if (keyCode == 0xb7) {
    keyCode = 0x54;
  }

  handleKeyRelease(keyCode);

  // Windows has a rather nasty bug where it won't send key release
  // events for a Shift button if the other Shift is still pressed
  if ((keyCode == 0x2a) || (keyCode == 0x36)) {
    if (m_downKeySym.count(0x2a)) {
      handleKeyRelease(0x2a);
    }
    if (m_downKeySym.count(0x36)) {
      handleKeyRelease(0x36);
    }
  }

  return 1;
}

void QVNCWinView::setQCursor(const QCursor &cursor)
{
  QImage image = cursor.pixmap().toImage();
  int width = image.width();
  int height = image.height();
  int hotX = cursor.hotSpot().x();
  int hotY = cursor.hotSpot().y();
  BITMAPV5HEADER header;
  memset(&header, 0, sizeof(BITMAPV5HEADER));
  header.bV5Size = sizeof(BITMAPV5HEADER);
  header.bV5Width = width;
  header.bV5Height = -height;
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;

  HDC hdc = GetDC(m_hwnd);
  quint32 *bits = nullptr;
  HBITMAP bitmap = CreateDIBSection(hdc, (BITMAPINFO*)&header, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
  ReleaseDC(nullptr, hdc);

  quint32* ptr = bits;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      *ptr++ = image.pixel(x, y);
    }
  }

  HBITMAP empty_mask = CreateBitmap(width, height, 1, 1, nullptr);
  ICONINFO icon_info;
  icon_info.fIcon = false;
  icon_info.xHotspot = hotX;
  icon_info.yHotspot = hotY;
  icon_info.hbmMask = empty_mask;
  icon_info.hbmColor = bitmap;

  DestroyIcon(m_cursor);
  m_cursor = CreateIconIndirect(&icon_info);
  DeleteObject(bitmap);
  DeleteObject(empty_mask);

  SetCursor(m_cursor);
}

void QVNCWinView::setCursorPos(int x, int y)
{
  if (!m_mouseGrabbed) {
    // Do nothing if we do not have the mouse captured.
    return;
  }
  SetCursorPos(x, y);
}

bool QVNCWinView::hasViewFocus() const
{
  return GetFocus() == m_hwnd;
}

void QVNCWinView::pushLEDState()
{
  qDebug() << "QVNCWinView::pushLEDState";
  // Server support?
  rfb::ServerParams *server = AppManager::instance()->connection()->server();
  if (server->ledState() == rfb::ledUnknown) {
    return;
  }

  unsigned int state = 0;
  if (GetKeyState(VK_CAPITAL) & 0x1) {
    state |= rfb::ledCapsLock;
  }
  if (GetKeyState(VK_NUMLOCK) & 0x1) {
    state |= rfb::ledNumLock;
  }
  if (GetKeyState(VK_SCROLL) & 0x1) {
    state |= rfb::ledScrollLock;
  }

  if ((state & rfb::ledCapsLock) != (server->ledState() & rfb::ledCapsLock)) {
    vlog.debug("Inserting fake CapsLock to get in sync with server");
    handleKeyPress(0x3a, XK_Caps_Lock);
    handleKeyRelease(0x3a);
  }
  if ((state & rfb::ledNumLock) != (server->ledState() & rfb::ledNumLock)) {
    vlog.debug("Inserting fake NumLock to get in sync with server");
    handleKeyPress(0x45, XK_Num_Lock);
    handleKeyRelease(0x45);
  }
  if ((state & rfb::ledScrollLock) != (server->ledState() & rfb::ledScrollLock)) {
    vlog.debug("Inserting fake ScrollLock to get in sync with server");
    handleKeyPress(0x46, XK_Scroll_Lock);
    handleKeyRelease(0x46);
  }
}

void QVNCWinView::setLEDState(unsigned int state)
{
  qDebug() << "QVNCWinView::setLEDState";
  vlog.debug("Got server LED state: 0x%08x", state);

  // The first message is just considered to be the server announcing
  // support for this extension. We will push our state to sync up the
  // server when we get focus. If we already have focus we need to push
  // it here though.
  if (m_firstLEDState) {
    m_firstLEDState = false;
    if (hasFocus()) {
      pushLEDState();
    }
    return;
  }

  if (!hasFocus()) {
    return;
  }

  INPUT input[6];
  memset(input, 0, sizeof(input));
  UINT count = 0;

  if (!!(state & rfb::ledCapsLock) != !!(GetKeyState(VK_CAPITAL) & 0x1)) {
    input[count].type = input[count+1].type = INPUT_KEYBOARD;
    input[count].ki.wVk = input[count+1].ki.wVk = VK_CAPITAL;
    input[count].ki.wScan = input[count+1].ki.wScan = SCAN_FAKE;
    input[count].ki.dwFlags = 0;
    input[count+1].ki.dwFlags = KEYEVENTF_KEYUP;
    count += 2;
  }

  if (!!(state & rfb::ledNumLock) != !!(GetKeyState(VK_NUMLOCK) & 0x1)) {
    input[count].type = input[count+1].type = INPUT_KEYBOARD;
    input[count].ki.wVk = input[count+1].ki.wVk = VK_NUMLOCK;
    input[count].ki.wScan = input[count+1].ki.wScan = SCAN_FAKE;
    input[count].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    input[count+1].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
    count += 2;
  }

  if (!!(state & rfb::ledScrollLock) != !!(GetKeyState(VK_SCROLL) & 0x1)) {
    input[count].type = input[count+1].type = INPUT_KEYBOARD;
    input[count].ki.wVk = input[count+1].ki.wVk = VK_SCROLL;
    input[count].ki.wScan = input[count+1].ki.wScan = SCAN_FAKE;
    input[count].ki.dwFlags = 0;
    input[count+1].ki.dwFlags = KEYEVENTF_KEYUP;
    count += 2;
  }

  if (count == 0) {
    return;
  }

  UINT ret = SendInput(count, input, sizeof(*input));
  if (ret < count) {
    vlog.error(_("Failed to update keyboard LED state: %lu"), GetLastError());
  }
}

void QVNCWinView::maybeGrabKeyboard()
{
  if (::fullscreenSystemKeys && isFullscreenEnabled() && hasFocus()) {
    grabKeyboard();
  }
}

void QVNCWinView::grabKeyboard()
{
  int ret = win32_enable_lowlevel_keyboard(m_hwnd);
  if (ret != 0) {
    vlog.error(_("Failure grabbing keyboard"));
    return;
  }
  QAbstractVNCView::grabKeyboard();
}

void QVNCWinView::ungrabKeyboard()
{
#if 0
  // maybe unnecessary
  ungrabPointer();
#endif
  win32_disable_lowlevel_keyboard(m_hwnd);
  QAbstractVNCView::ungrabKeyboard();
}

void QVNCWinView::bell()
{
  MessageBeep(0xFFFFFFFF); // cf. fltk/src/drivers/WinAPI/Fl_WinAPI_Screen_Driver.cxx:245
}

void QVNCWinView::startMouseTracking()
{
  if (!m_mouseTracking) {
    // Enable mouse tracking.
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.hwndTrack = m_hwnd;
    tme.dwFlags = TME_HOVER | TME_LEAVE;
    tme.dwHoverTime = HOVER_DEFAULT;
    TrackMouseEvent(&tme);
    m_mouseTracking = true;

    SetCursor(NULL);
    qDebug() << "SetCursor(NULL)";

    if (m_keyboardGrabbed) {
      grabPointer();
    }
  }
}

void QVNCWinView::stopMouseTracking()
{
  m_mouseTracking = false;
  SetCursor(m_defaultCursor);
}

void QVNCWinView::moveView(int x, int y)
{
  MoveWindow((HWND)window()->winId(), x, y, width(), height(), false);
}

void QVNCWinView::updateWindow()
{
  QAbstractVNCView::updateWindow();
  refresh(m_hwnd);
}
