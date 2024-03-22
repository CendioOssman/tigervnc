#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QCursor>
#include <qt_windows.h>
#include <windowsx.h>

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
#include "Win32TouchHandler.h"
#include "Win32KeyboardHandler.h"
#include "win32.h"
#include "i18n.h"
#include "vncwindow.h"
#include "vncwinview.h"

#include <QDebug>
#include <QMessageBox>
#include <QTime>
#include <QScreen>

static rfb::LogWriter vlog("Viewport");

// Used to detect fake input (0xaa is not a real key)
static const WORD SCAN_FAKE = 0xaa;
static const WORD NoSymbol = 0;

QVNCWinView::QVNCWinView(QWidget *parent, Qt::WindowFlags f)
 : QAbstractVNCView(parent, f)
 , altGrArmed_(false)
 , altGrCtrlTimer_(new QTimer)
 , cursor_(nullptr)
 , mouseTracking_(false)
 , defaultCursor_(LoadCursor(NULL, IDC_ARROW))
 , touchHandler_(nullptr)
{
  // Do not set either Qt::WA_NoBackground nor Qt::WA_NoSystemBackground
  // for Windows. Otherwise, unneeded ghost image of the toast is shown
  // when dimming.
  setAttribute(Qt::WA_InputMethodTransparent);
  setAttribute(Qt::WA_AcceptTouchEvents);

  grabGesture(Qt::TapGesture);
  grabGesture(Qt::TapAndHoldGesture);
  grabGesture(Qt::PanGesture);
  grabGesture(Qt::PinchGesture);
  grabGesture(Qt::SwipeGesture);
  grabGesture(Qt::CustomGesture);

  setFocusPolicy(Qt::StrongFocus);

  altGrCtrlTimer_->setInterval(100);
  altGrCtrlTimer_->setSingleShot(true);
  connect(altGrCtrlTimer_, &QTimer::timeout, this, [this]() {
    altGrArmed_ = false;
      keyboardHandler_->handleKeyPress(0x1d, XK_Control_L);
  });

  keyboardHandler_ = new Win32KeyboardHandler(this);
  initKeyboardHandler();
}

QVNCWinView::~QVNCWinView()
{
  altGrCtrlTimer_->stop();
  delete altGrCtrlTimer_;

  DestroyIcon(cursor_);

  delete touchHandler_;
}

bool QVNCWinView::event(QEvent *e)
{
  try {
    switch(e->type()) {
    case QEvent::CursorChange:
      //qDebug() << "CursorChange";
      e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
      break;
    default:
      //qDebug() << "Unprocessed Event: " << e->type();
      break;
    }
    return QWidget::event(e);
  }
  catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    AppManager::instance()->publishError(e.str(), true);
    return false;
  }
}

void QVNCWinView::enterEvent(QEvent *e)
{
  qDebug() << "QVNCWinView::enterEvent";
  grabPointer();
  //SetCursor(cursor_);
  QWidget::enterEvent(e);
}

void QVNCWinView::leaveEvent(QEvent *e)
{
  qDebug() << "QVNCWinView::leaveEvent";
  ungrabPointer();
  QWidget::leaveEvent(e);
}

void QVNCWinView::resizeEvent(QResizeEvent *e)
{
  QVNCWindow *window = AppManager::instance()->window();
  QSize vsize = window->viewport()->size();
  qDebug() << "QVNCWinView::resizeEvent: w=" << e->size().width() << ", h=" << e->size().height() << ", viewport=" << vsize;

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
  grabPointer();
  maybeGrabKeyboard();
}

int QVNCWinView::handleTouchEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
  return touchHandler_->processEvent(message, wParam, lParam);
}

void QVNCWinView::setCursorPos(int x, int y)
{
  if (!mouseGrabbed_) {
    // Do nothing if we do not have the mouse captured.
    return;
  }
  QPoint gp = mapToGlobal(QPoint(x, y));
  qDebug() << "QVNCWinView::setCursorPos: local xy=" << x << y << ", screen xy=" << gp.x() << gp.y();
  x = gp.x();
  y = gp.y();
  SetCursorPos(x, y);
}

void QVNCWinView::ungrabKeyboard()
{
  ungrabPointer();
  QAbstractVNCView::ungrabKeyboard();
}

void QVNCWinView::bell()
{
  MessageBeep(0xFFFFFFFF); // cf. fltk/src/drivers/WinAPI/Fl_WinAPI_Screen_Driver.cxx:245
}

void QVNCWinView::moveView(int x, int y)
{
  MoveWindow((HWND)window()->winId(), x, y, width(), height(), false);
}

QRect QVNCWinView::getExtendedFrameProperties()
{
  // Returns Windows10's magic number. This method might not be necessary for Qt6 / Windows11.
  // See the followin URL for more details.
  // https://stackoverflow.com/questions/42473554/windows-10-screen-coordinates-are-offset-by-7
  return QRect(7, 7, 7, 7);
}
