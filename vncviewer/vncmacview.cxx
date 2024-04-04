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
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);

  keyboardHandler = new MacKeyboardHandler(this);
  initKeyboardHandler();
}

QVNCMacView::~QVNCMacView() {}

bool QVNCMacView::event(QEvent* e)
{
  switch (e->type()) {
  case QEvent::WindowActivate:
    // qDebug() << "WindowActivate";
    grabPointer();
    break;
  case QEvent::WindowDeactivate:
    // qDebug() << "WindowDeactivate";
    ungrabPointer();
    break;
  case QEvent::Enter:
  case QEvent::FocusIn:
    // qDebug() << "Enter/FocusIn";
    setFocus();
    grabPointer();
    break;
  case QEvent::Leave:
  case QEvent::FocusOut:
    // qDebug() << "Leave/FocusOut";
    clearFocus();
    ungrabPointer();
    break;
  case QEvent::CursorChange:
    // qDebug() << "CursorChange";
    e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    return true;
  case QEvent::MetaCall:
    // qDebug() << "QEvent::MetaCall (signal-slot call)";
    break;
  default:
    // qDebug() << "Unprocessed Event: " << e->type();
    break;
  }
  return QWidget::event(e);
}

void QVNCMacView::bell()
{
  cocoa_beep();
}
