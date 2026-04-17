#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "KeyboardMacOS.h"
#include "cocoa.h"
#include "rfb/LogWriter.h"
#include "vncmacview.h"

#include <QApplication>
#include <QDataStream>
#include <QDebug>
#include <QEvent>
#include <QTextStream>

static rfb::LogWriter vlog("QVNCMacView");

QVNCMacView::QVNCMacView(CConn* cc_, QWidget* parent, Qt::WindowFlags f)
  : Viewport(cc_, parent, f)
{
  keyboard = new KeyboardMacOS(this);
  initKeyboardHandler();
}

QVNCMacView::~QVNCMacView() {}

bool QVNCMacView::event(QEvent* e)
{
  return Viewport::event(e);
}
