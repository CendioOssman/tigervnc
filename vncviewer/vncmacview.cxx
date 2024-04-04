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
  : QAbstractVNCView(parent, f)
{
  keyboardHandler = new MacKeyboardHandler(this);
  initKeyboardHandler();
}

QVNCMacView::~QVNCMacView() {}

bool QVNCMacView::event(QEvent* e)
{
  return QAbstractVNCView::event(e);
}

void QVNCMacView::bell()
{
  cocoa_beep();
}
