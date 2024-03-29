#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QCursor>
#include <QEvent>
#include <QResizeEvent>
#include <QTimer>
#include <qt_windows.h>
#include <windowsx.h>

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "PlatformPixelBuffer.h"
#include "Win32KeyboardHandler.h"
#include "Win32TouchHandler.h"
#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "rdr/Exception.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/keysymdef.h"
#include "rfb/ledStates.h"
#include "vncconnection.h"
#include "vncwindow.h"
#include "vncwinview.h"
#include "win32.h"

#include <QDebug>
#include <QMessageBox>
#include <QScreen>
#include <QTime>

static rfb::LogWriter vlog("Viewport");

// Used to detect fake input (0xaa is not a real key)
static const WORD SCAN_FAKE = 0xaa;
static const WORD NoSymbol = 0;

QVNCWinView::QVNCWinView(QWidget* parent, Qt::WindowFlags f)
  : QAbstractVNCView(parent, f)
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

  keyboardHandler = new Win32KeyboardHandler(this);
  initKeyboardHandler();
}

QVNCWinView::~QVNCWinView() {}

bool QVNCWinView::event(QEvent* e)
{
  try {
    switch (e->type()) {
    case QEvent::CursorChange:
      // qDebug() << "CursorChange";
      e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
      break;
    default:
      // qDebug() << "Unprocessed Event: " << e->type();
      break;
    }
    return QWidget::event(e);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    AppManager::instance()->publishError(e.str(), true);
    return false;
  }
}

void QVNCWinView::enterEvent(QEvent* e)
{
  qDebug() << "QVNCWinView::enterEvent";
  grabPointer();
  // SetCursor(cursor_);
  QWidget::enterEvent(e);
}

void QVNCWinView::leaveEvent(QEvent* e)
{
  qDebug() << "QVNCWinView::leaveEvent";
  ungrabPointer();
  QWidget::leaveEvent(e);
}

int QVNCWinView::handleTouchEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
  return touchHandler->processEvent(message, wParam, lParam);
}

void QVNCWinView::setCursorPos(int x, int y)
{
  qDebug() << "QVNCWinView::setCursorPos" << mouseGrabbed;
  if (!mouseGrabbed) {
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
