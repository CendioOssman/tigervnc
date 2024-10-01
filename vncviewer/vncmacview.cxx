#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "MacKeyboardHandler.h"
#include "appmanager.h"
#include "cocoa.h"
#include "rfb/LogWriter.h"
#include "vncmacview.h"

#include <QApplication>
#include <QDataStream>
#include <QDebug>
#include <QEvent>
#include <QTextStream>

static rfb::LogWriter vlog("QVNCMacView");

QVNCMacView::QVNCMacView(QWidget* parent, Qt::WindowFlags f)
  : Viewport(parent, f)
{
  keyboardHandler = new MacKeyboardHandler(this);
  initKeyboardHandler();
}

QVNCMacView::~QVNCMacView() {}

void QVNCMacView::setCursorPos(int x, int y)
{
  vlog.debug("QVNCMacView::setCursorPos mouseGrabbed=%d", mouseGrabbed);
  vlog.debug("QVNCMacView::setCursorPos keyboardGrabbed=%d", keyboardHandler->isKeyboardGrabbed());
  if (!mouseGrabbed || !keyboardHandler->isKeyboardGrabbed()) {
    // Do nothing if we do not have the mouse captured.
    return;
  }
  QPoint gp = mapToGlobal(localPointAdjust(QPoint(x, y)));
  vlog.debug("QVNCMacView::setCursorPos local x=%d y=%d", x, y);
  vlog.debug("QVNCMacView::setCursorPos screen x=%d y=%d", gp.x(), gp.y());
  cocoa_set_cursor_pos(gp.x(), gp.y());
}

bool QVNCMacView::event(QEvent* e)
{
  return Viewport::event(e);
}

void QVNCMacView::bell()
{
  cocoa_beep();
}
