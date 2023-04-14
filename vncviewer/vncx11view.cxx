#include <QEvent>
#include <QTextStream>
#include <QDataStream>
#include <QUrl>
#if defined(WIN32)
#include <qt_windows.h>
#endif
#include "rfb/ServerParams.h"
#include "parameters.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"
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
#include <X11/extensions/Xrender.h>

#pragma pack (1)
struct BITMAPFILEHEADER
{
  short    bfType;
  int    bfSize;
  short    bfReserved1;
  short    bfReserved2;
  int   bfOffBits;
};

struct BITMAPINFOHEADER
{
  int  biSize;
  int   biWidth;
  int   biHeight;
  short   biPlanes;
  short   biBitCount;
  int  biCompression;
  int  biSizeImage;
  int   biXPelsPerMeter;
  int   biYPelsPerMeter;
  int  biClrUsed;
  int  biClrImportant;
};

void saveXImageToBitmap(XImage *pImage)
{
  BITMAPFILEHEADER bmpFileHeader;
  BITMAPINFOHEADER bmpInfoHeader;
  memset(&bmpFileHeader, 0, sizeof(BITMAPFILEHEADER));
  memset(&bmpInfoHeader, 0, sizeof(BITMAPINFOHEADER));
  bmpFileHeader.bfType = 0x4D42;
  bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bmpFileHeader.bfReserved1 = 0;
  bmpFileHeader.bfReserved2 = 0;
  int biBitCount = 32;
  int dwBmpSize = ((pImage->width * biBitCount + 31) / 32) * 4 * pImage->height;
  bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +  dwBmpSize;

  bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmpInfoHeader.biWidth = pImage->width;
  bmpInfoHeader.biHeight = pImage->height;
  bmpInfoHeader.biPlanes = 1;
  bmpInfoHeader.biBitCount = biBitCount;
  bmpInfoHeader.biSizeImage = 0;
  bmpInfoHeader.biCompression = 0;
  bmpInfoHeader.biXPelsPerMeter = 0;
  bmpInfoHeader.biYPelsPerMeter = 0;
  bmpInfoHeader.biClrUsed = 0;
  bmpInfoHeader.biClrImportant = 0;

  static int cnt = 0;
  char filePath[255];
  sprintf(filePath, "bitmap%d.bmp", cnt++);
  FILE *fp = fopen(filePath, "wb");

  if(fp == NULL)
    return;

  fwrite(&bmpFileHeader, sizeof(bmpFileHeader), 1, fp);
  fwrite(&bmpInfoHeader, sizeof(bmpInfoHeader), 1, fp);
  fwrite(pImage->data, dwBmpSize, 1, fp);
  fclose(fp);
}

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
QVNCX11View::QVNCX11View(QWidget *parent, Qt::WindowFlags f)
  : QAbstractVNCView(parent, f)
  , m_window(0)
  , m_region(new rfb::Region)
{
  setAttribute(Qt::WA_NoBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance(), &AppManager::invalidateRequested, this, &QVNCX11View::addInvalidRegion, Qt::QueuedConnection);
}

/*!
    Destroys the VNCX11view object. If the hosted Win32 window has not
    been set explicitly using setWindow() the window will be
    destroyed.
*/
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

char *loadbmpfile(const char *filename, int *width, int *height, int *bitcount, int *linelen)
{
    char *bmpdata = nullptr;
    int i, size;
    int off, hs, w = 0, h = 0, bcnt = 0;
    int line = 0;
    int pl = 0, bicomp = 0, clrused;
    char bm[3];
    FILE *fi = fopen(filename,"rb");
    bcnt = 0;
    if (fi) {
        fread(bm, 1, 2, fi);
        fread(&i , 4, 1, fi);//fsize
        fread(&i , 2, 2, fi);
        fread(&off , 4, 1, fi);//offset
        fread(&hs , 4, 1, fi);//hdsize
        if (hs == 40) {
            fread(&w , 4, 1, fi);//width
            fread(&h , 4, 1, fi);//height
            fread(&pl , 2, 1, fi);//planes
            fread(&bcnt , 2, 1, fi);//bitcount
            fread(&bicomp, 4, 1, fi);//biComp
            fread(&i , 4, 1, fi);//biSizeImage
            fread(&i , 4, 1, fi);//biXPixPerMeter
            fread(&i , 4, 1, fi);//biYPixPerMeter
            fread(&clrused , 4, 1, fi);//biClrUsed
            fread(&i , 4, 1, fi);//biClrImpotant
        } else {
            fread(&w , 2, 1, fi);//width
            fread(&h , 2, 1, fi);//height
            fread(&pl , 2, 1, fi);//planes
            fread(&bcnt , 2, 1, fi);//bitcount
        }
        line = w * (bcnt / 8);
        line = ((line + 3) / 4) * 4;
        size = line * h;
        if (size < 0) size = -size;
        bmpdata = (char*)malloc(size);
        fread(bmpdata, 1, size, fi);

        fclose(fi);
    }
    *bitcount = bcnt;
    *width = w;
    *height = h;
    *linelen = line;
    return bmpdata;
}

XImage *load_bmp(Display *d, const char *filename, int *imwidth, int *imheight) {
    char *bmpdata;
    int depth = XDefaultDepth(d, 0);
    int width, height, bitcount, linelen, bpp;
    XImage *img;
    char *data;
    int i, j, h, pixbytes;
    bmpdata = loadbmpfile(filename, &width, &height, &bitcount, &linelen);
    pixbytes = bitcount / 8;
    bpp = depth >= 24 ? 4 : 2;
    h = height;
    if (height < 0) height = -height;
    data = (char*)calloc(bpp, height * width);
    for (i = 0; i < height; i++) {
        int k = h < 0 ? i * linelen : (height - i -1) * linelen;
        int m = i * width * bpp;
        for (j = 0; j < width; j++) {
            int r = k + j * pixbytes;
            int s = m + j * bpp;
            memcpy(data+s, bmpdata+r, pixbytes);
        }
    }
    free(bmpdata);
    img = XCreateImage( d, CopyFromParent, depth, ZPixmap,
            0, data, width, height, bpp*8, bpp*width );
    *imwidth = width;
    *imheight = height;
    return img;
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
  qDebug() << "QQVNCX11View::addInvalidRegion: x=" << r.tl.x << ", y=" << r.tl.y << ", w=" << (r.br.x-r.tl.x) << ", h=" << (r.br.y-r.tl.y);

  // copy the specified region in XImage (== data in framebuffer) to Pixmap.
  //GC gc = framebuffer->gc();
  Pixmap pixmap = framebuffer->pixmap();
  XGCValues gcvalues;
  GC gc = XCreateGC(display(), pixmap, 0, &gcvalues);
#if 1
  XImage *xim = framebuffer->ximage();
#else
  int iwidth, iheight;
  XImage *xim = load_bmp(display(), "tiger.bmp", &iwidth, &iheight);
  x1 = x0 + iwidth;
  y1 = y0 + iheight;
  w = x1 - x0;
  h = y1 - y0;
#endif
  XShmSegmentInfo *shminfo = framebuffer->shmSegmentInfo();
  if (shminfo) {
    int ret = XShmPutImage(display(), pixmap, gc, xim, x0, y0, x0, y0, w, h, False);
    //int ret = XShmPutImage(display(), m_window, gc, xim, x0, y0, x0, y0, w, h, False);
    qDebug() << "XShmPutImage: ret=" << ret;
    // Need to make sure the X server has finished reading the
    // shared memory before we return
    XSync(display(), False);
  } else {
    int ret = XPutImage(display(), pixmap, gc, xim, x0, y0, x0, y0, w, h);
    qDebug() << "XPutImage(pixmap):ret=" << ret;
//    ret = XPutImage(display(), m_window, gc, xim, x0, y0, x0, y0, w, h);
//    qDebug() << "XPutImage(window):ret=" << ret;
  }
  //const char* str1 = "################# QVNCX11View::updateWindow 1 ##############";
  //const char* str2 = "################# QVNCX11View::updateWindow 2 ##############";
  //XDrawString(display(), m_window, gc, 200, 100, str1, strlen(str1));
  //XDrawString(display(), pixmap, gc, 200, 200, str2, strlen(str2));

  XFreeGC(display(), gc);

  update(x0, y0, w, h);

#if 0
  if (w > 500) {
    saveXImageToBitmap(xim);
  }
#endif
}

void QVNCX11View::updateWindow()
{
  QAbstractVNCView::updateWindow();
  // Nothing more to do, because invalid regions are notified to Qt by addInvalidRegion().
#if 0
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect r = framebuffer->getDamage();
  qDebug() << "QVNCX11View::updateWindow: x=" << r.tl.x << ", y=" << r.tl.y << ", w=" << (r.br.x-r.tl.x) << ", h=" << (r.br.y-r.tl.y);
  int w = r.width();
  int h = r.height();
  if (w <= 0 || h <= 0) {
    return;
  }
  int x0 = r.tl.x;
  int y0 = r.tl.y;
  int x1 = r.br.x;
  int y1 = r.br.y;

  Pixmap pixmap = framebuffer->pixmap();
#if 0
  // not working.
  XImage *xim = framebuffer->ximage();
#else
  // work well. (only on m_window)
  static char data[50 * 50 * 4];
  memset(data, 250, 50 * 50 * 4);
#if 0
  XImage *xim = XCreateImage(
        display(),
        CopyFromParent,
        DefaultDepth(display(),DefaultScreen(display())),
        ZPixmap,
        0,
        data,
        50,
        50,
        32,
        50 * 4);
  x1 = x0 + 50;
  y1 = y0 + 50;
#else
  int iwidth, iheight;
  XImage *xim = load_bmp(display(), "tiger.bmp", &iwidth, &iheight);
  x1 = x0 + iwidth;
  y1 = y0 + iheight;
#endif
#endif

  XShmSegmentInfo *shminfo = framebuffer->shmSegmentInfo();
  GC gc = framebuffer->gc();
  //GC gc = DefaultGC(display(), 0);
  //GC gc = XCreateGC(display(), pixmap, 0, NULL);
  if (shminfo) {
    int ret = XShmPutImage(display(), pixmap, gc, xim, x0, y0, x1, y1, w, h, False);
    //int ret = XShmPutImage(display(), m_window, gc, xim, x0, y0, x1, y1, w, h, False);
    qDebug() << "XShmPutImage: ret=" << ret;
    // Need to make sure the X server has finished reading the
    // shared memory before we return
    XSync(display(), False);
  } else {
    int ret = XPutImage(display(), pixmap, gc, xim, x0, y0, x1, y1, w, h);
    //int ret = XPutImage(display(), m_window, gc, xim, x0, y0, x1, y1, w, h);
    qDebug() << "XPutImage :ret=" << ret;
  }
  const char* str1 = "################# QVNCX11View::updateWindow 1 ##############";
  const char* str2 = "################# QVNCX11View::updateWindow 2 ##############";
  XDrawString(display(), m_window, gc, 200, 100, str1, strlen(str1));
  XDrawString(display(), pixmap, gc, 200, 100, str2, strlen(str2));
  //XFreeGC(display(), gc);
#endif
}

/*!
    \reimp
*/
bool QVNCX11View::event(QEvent *e)
{
  switch(e->type()) {
    case QEvent::Polish:
      if (!m_window) {
        qDebug() << "display numbers:  QX11Info::display()=" <<  QX11Info::display() << ", XOpenDisplay(NULL)=" << XOpenDisplay(NULL);
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
        m_window = XCreateWindow(display(), RootWindow(display(), screenNumber), 0, 0, w, h, borderWidth, 32, InputOutput, framebuffer->visualInfo()->visual, CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap, &xattr);
        XReparentWindow(display(), m_window, winId(), 0, 0);
        XMapWindow(display(), m_window);
      }
      break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick: {
      QMouseEvent *mev = (QMouseEvent*)e;
      qDebug() << "QVNCX11View::event: x=" << mev->x() << ",y=" << mev->y() << ",button=" << Qt::hex << mev->button();

      //GC gc = DefaultGC(display(), 0);
      QVNCConnection *cc = AppManager::instance()->connection();
      PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
      GC gc = framebuffer->gc();
//      const char* str = "ABCDEFG ################# Hello ##############";
//      XDrawString(display(), m_window, gc, mev->x() , mev->y(), str, strlen(str));

//      int iwidth, iheight;
//      XImage *image = load_bmp(display(), "tiger.bmp", &iwidth, &iheight);
//      XPutImage(display(), m_window, gc, image, 0, 0, mev->x() , mev->y(), iwidth, iheight);
      //QVNCConnection *cc = AppManager::instance()->connection();
      //PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
      XImage *image = framebuffer->ximage();
      XPutImage(display(), m_window, gc, image, 0, 0, 0, 0, image->width, image->height);
      XSync(display(), False);
      }
      break;
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
      break;
    case QEvent::WindowDeactivate:
      //qDebug() << "WindowDeactivate";
      break;
    case QEvent::Enter:
      //qDebug() << "Enter";
      break;
    case QEvent::Leave:
      //qDebug() << "Leave";
      break;
    case QEvent::CursorChange:
      //qDebug() << "CursorChange";
      e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    case QEvent::Paint:
      qDebug() << "QEvent::Paint";
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
  XResizeWindow(display(), m_window, e->size().width(), e->size().height());
  QWidget::resizeEvent(e);

  if (m_window) {
//    QSize size = e->size();
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
    uint16_t xcbEventType = ev->response_type & ~0x80;
    //qDebug() << "QVNCX11View::nativeEvent: xcbEventType=" << xcbEventType << ",eventType=" << eventType;
    if (xcbEventType == XCB_GE_GENERIC) {
      xcb_ge_generic_event_t* genericEvent = static_cast<xcb_ge_generic_event_t*>(message);
      xcbEventType = genericEvent->event_type;
      //qDebug() << "QVNCX11View::nativeEvent: XCB_GE_GENERIC: xcbEventType=" << xcbEventType;
    }

    // seems not working.
//    if (xcbEventType == XCB_BUTTON_PRESS) {
//      xcb_button_press_event_t* buttonPressEvent = static_cast<xcb_button_press_event_t*>(message);
//      qDebug() << "QVNCX11View::nativeEvent: XCB_BUTTON_PRESS: x=" << buttonPressEvent->root_x << ",y=" << buttonPressEvent->root_y << ",button=" << Qt::hex << buttonPressEvent->detail;
//    }
    if (xcbEventType == XCB_KEY_PRESS) {
        xcb_key_press_event_t* keyPressEvent = reinterpret_cast<xcb_key_press_event_t*>(ev);
        qDebug() << "QVNCX11View::nativeEvent: XCB_KEY_PRESS: key=0x" << Qt::hex << keyPressEvent->detail;
    }
    if (xcbEventType == XCB_EXPOSE) {

    }
  }
  return false;
  //return QWidget::nativeEvent(eventType, message, result);
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
  qDebug() << "QVNCX11View::draw: x=" << x0 << ", y=" << y0 << ", w=" << w << ", h=" << h;
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  framebuffer->draw(x0, y0, x0, y0, w, h);

  m_region->clear();
}
