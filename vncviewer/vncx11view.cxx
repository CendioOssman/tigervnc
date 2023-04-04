#include <QEvent>
#include <qt_windows.h>
#include "appmanager.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"
#include "vncx11view.h"

#include <XLib.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#endif

#include <QDebug>

/*!
    \class VNCX11view qwinhost.h
    \brief The VNCX11view class provides an API to use native Win32
    windows in Qt applications.

    VNCX11view exists to provide a QWidget that can act as a parent for
    any native Win32 control. Since VNCX11view is a proper QWidget, it
    can be used as a toplevel widget (e.g. 0 parent) or as a child of
    any other QWidget.

    VNCX11view integrates the native control into the Qt user interface,
    e.g. handles focus switches and laying out.

    Applications moving to Qt may have custom Win32 controls that will
    take time to rewrite with Qt. Such applications can use these
    custom controls as children of VNCX11view widgets. This allows the
    application's user interface to be replaced gradually.

    When the VNCX11view is destroyed, and the Win32 window hasn't been
    set with setWindow(), the window will also be destroyed.
*/

/*!
    Creates an instance of VNCX11view. \a parent and \a f are
    passed on to the QWidget constructor. The widget has by default
    no background.

    \warning You cannot change the parent widget of the VNCX11view instance
    after the native window has been created, i.e. do not call
    QWidget::setParent or move the VNCX11view into a different layout.
*/
QVNCX11view::QVNCX11view(QWidget *parent, Qt::WindowFlags f)
  : QAbstractVNCView(parent, f)
  , m_wndproc(0)
  , m_hwndowner(false)
  , m_hwnd(0)
  , m_rect(new rfb::Rect)
{
  setAttribute(Qt::WA_NoBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance(), &AppManager::updateRequested, this, [this](int x0, int y0, int x1, int y1) {
#if defined(WIN32)
    RECT r{x0, y0, x1, y1};
    InvalidateRect(m_hwnd, &r, false); // otherwise, UpdateWindow(m_hwnd);
#endif
//    m_rect->union_boundary(rfb::Rect(x0, y0, x1, y1));
  });
}

/*!
    Destroys the VNCX11view object. If the hosted Win32 window has not
    been set explicitly using setWindow() the window will be
    destroyed.
*/
QVNCX11view::~QVNCX11view()
{
  if (m_wndproc) {
    SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_wndproc);
  }

  if (m_hwnd && m_hwndowner) {
    DestroyWindow(m_hwnd);
  }
  delete m_rect;
}

/*!
    Reimplement this virtual function to create and return the native
    Win32 window. \a parent is the handle to this widget, and \a
    instance is the handle to the application instance. The returned HWND
    must be a child of the \a parent HWND.

    The default implementation returns null. The window returned by a
    reimplementation of this function is owned by this VNCX11view
    instance and will be destroyed in the destructor.

    This function is called by the implementation of polish() if no
    window has been set explicitly using setWindow(). Call polish() to
    force this function to be called.

    \sa setWindow()
*/
HWND QVNCX11view::createWindow(HWND parent, HINSTANCE instance)
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
    wcex.hCursor	= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= "VNC Window";
    wcex.hIconSm	= NULL;

    windowClass = RegisterClassEx(&wcex);
  }

  HWND hwnd = CreateWindow("VNC Window", 0, WS_CHILD|WS_CLIPSIBLINGS|WS_TABSTOP,
                           CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, parent, NULL, instance, NULL);

  return hwnd;
}

/*!
    Ensures that the window provided a child of this widget, unless
    it is a WS_OVERLAPPED window.
*/
void QVNCX11view::fixParent()
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

/*!
    Sets the native Win32 window to \a window. If \a window is not a child
    window of this widget, then it is reparented to become one. If \a window
    is not a child window (i.e. WS_OVERLAPPED is set), then this function does nothing.

    The lifetime of the window handle will be managed by Windows, VNCX11view does not
    call DestroyWindow. To verify that the handle is destroyed when expected, handle
    WM_DESTROY in the window procedure.

    \sa window(), createWindow()
*/
void QVNCX11view::setWindow(HWND window)
{
  if (m_hwnd && m_hwndowner) {
    DestroyWindow(m_hwnd);
  }
  m_hwnd = window;
  fixParent();

  m_hwndowner = false;
}

/*!
    Returns the handle to the native Win32 window, or null if no
    window has been set or created yet.

    \sa setWindow(), createWindow()
*/
HWND QVNCX11view::window() const
{
  return m_hwnd;
}

void *getWindowProc(QVNCX11view *host)
{
  return host ? host->m_wndproc : 0;
}

LRESULT CALLBACK QVNCX11view::eventHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  QWidget *widget = QWidget::find((WId)GetParent(hWnd));
  QVNCX11view *window = qobject_cast<QVNCX11view*>(widget);

  if (window) {
    switch (message) {
      case WM_SETFOCUS:
        emit window->message("SetFocus for Win32 window!", 1000);
        break;
      case WM_KILLFOCUS:
        emit window->message("KillFocus for Win32 window!", 1000);
        break;
      case WM_MOUSEMOVE:
        emit window->message("Moving the mouse, aren't we?", 200);
        break;
      case WM_KEYDOWN:
        if (wParam != VK_TAB) {
          emit window->message("Key Pressed!", 500);
        }
        break;
      case WM_PAINT:
        window->refresh(hWnd);
        break;
      default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
  }
  return 0;
}

void QVNCX11view::refresh(HWND hWnd, bool all)
{
  qDebug() << "VNCX11view::refresh(): hWnd=" << hWnd;
  PlatformPixelBuffer *framebuffer = (PlatformPixelBuffer *)AppManager::instance()->connection()->framebuffer();
  PAINTSTRUCT ps;
  HDC hDC = BeginPaint(hWnd, &ps);
  HBITMAP hBitmap = framebuffer->hbitmap();
  BITMAP bitmap;
  GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bitmap);
  int x, y, width, height;
  if (all) {
    x = 0;
    y = 0;
    width = bitmap.bmWidth;
    height = bitmap.bmHeight;
  }
  else {
    x = ps.rcPaint.left;
    y = ps.rcPaint.top;
    width = ps.rcPaint.right - ps.rcPaint.left;
    height = ps.rcPaint.bottom - ps.rcPaint.top;
  }
  HDC hDCBits = CreateCompatibleDC(hDC);
  SelectObject(hDCBits, hBitmap);
  BitBlt(hDC, x, y, width, height, hDCBits, x, y, SRCCOPY);
  DeleteDC(hDCBits);
  EndPaint(hWnd, &ps);

  m_rect->clear();
}

void QVNCX11view::returnPressed()
{
  QMessageBox::information(topLevelWidget(), "Message from Qt", "Return pressed in QLineEdit!");
}

/*!
    \reimp
*/
bool QVNCX11view::event(QEvent *e)
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
        if (style & WS_TABSTOP)
          setFocusPolicy(Qt::FocusPolicy(focusPolicy() | Qt::StrongFocus));
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
    default:
      break;
  }
  return QWidget::event(e);
}

/*!
    \reimp
*/
void QVNCX11view::showEvent(QShowEvent *e)
{
  QWidget::showEvent(e);

  if (m_hwnd) {
    SetWindowPos(m_hwnd, HWND_TOP, 0, 0, width(), height(), SWP_SHOWWINDOW);
  }
}

/*!
    \reimp
*/
void QVNCX11view::focusInEvent(QFocusEvent *e)
{
  QWidget::focusInEvent(e);

  if (m_hwnd) {
    ::SetFocus(m_hwnd);
  }
}

/*!
    \reimp
*/
void QVNCX11view::resizeEvent(QResizeEvent *e)
{
  QWidget::resizeEvent(e);

  if (m_hwnd) {
    SetWindowPos(m_hwnd, HWND_TOP, 0, 0, width(), height(), 0);
  }
}

/*!
    \reimp
*/
bool QVNCX11view::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
  MSG *msg = (MSG *)message;
  switch (msg->message) {
    case WM_SETFOCUS:
      if (m_hwnd) {
        ::SetFocus(m_hwnd);
        return true;
      }
    default:
      break;
  }
  return QWidget::nativeEvent(eventType, message, result);
}

void QVNCX11view::bell()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Display *display = QX11Info::display();
#else
    QNativeInterface::QX11Application *f = (QNativeInterface::QX11Application *)QGuiApplication::nativeInterface<QNativeInterface::QX11Application>();
    Display *display f->display();
#endif
    extern int XBell(Display*, int);
    XBell(display, 0 /* volume */);
}
