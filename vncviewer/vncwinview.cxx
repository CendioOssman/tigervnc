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
#include "KeyboardWin32.h"
#include "appmanager.h"
#include "rfb/LogWriter.h"
#include "vncwinview.h"

#include <QMessageBox>
#include <QScreen>
#include <QTime>

static rfb::LogWriter vlog("Viewport");

QVNCWinView::QVNCWinView(QWidget* parent, Qt::WindowFlags f)
  : Viewport(parent, f)
{
  keyboardHandler = new KeyboardWin32(this);
  initKeyboardHandler();
}

QVNCWinView::~QVNCWinView() {}

bool QVNCWinView::event(QEvent* e)
{
  return Viewport::event(e);
}

void QVNCWinView::ungrabKeyboard()
{
  ungrabPointer();
  Viewport::ungrabKeyboard();
}

void QVNCWinView::bell()
{
  MessageBeep(0xFFFFFFFF); // cf. fltk/src/drivers/WinAPI/Fl_WinAPI_Screen_Driver.cxx:245
}
