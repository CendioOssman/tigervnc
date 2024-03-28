#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QApplication>
#include <QBitmap>
#include <QDebug>
#include <QGestureEvent>
#include <QGestureRecognizer>
#include <QImage>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif

#include "GestureHandler.h"
#include "PlatformPixelBuffer.h"
#include "X11KeyboardHandler.h"
#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "rfb/CMsgWriter.h"
#include "rfb/Exception.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/ledStates.h"
#include "vncconnection.h"
#include "vncgesturerecognizer.h"
#include "vncwindow.h"
#include "vncx11view.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xrender.h>

extern const struct _code_map_xkb_to_qnum {
  const char* from;
  const unsigned short to;
} code_map_xkb_to_qnum[];

extern const unsigned int code_map_xkb_to_qnum_len;

static int code_map_keycode_to_qnum[256];

static rfb::LogWriter vlog("QVNCX11View");

QVNCGestureRecognizer* QVNCX11View::vncGestureRecognizer_ = nullptr;

QVNCX11View::QVNCX11View(QWidget* parent, Qt::WindowFlags f)
  : QAbstractVNCView(parent, f)
  , gestureHandler(new GestureHandler)
#if 0
  , keyboardGrabberTimer_(new QTimer)
#endif
{
  if (!vncGestureRecognizer_) {
    vncGestureRecognizer_ = new QVNCGestureRecognizer;
  }
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);

  grabGesture(Qt::TapGesture);
  grabGesture(Qt::TapAndHoldGesture);
  grabGesture(Qt::PanGesture);
  grabGesture(Qt::PinchGesture);
  grabGesture(Qt::SwipeGesture);
  grabGesture(Qt::CustomGesture);
  QGestureRecognizer::registerRecognizer(vncGestureRecognizer_);

  keyboardHandler = new X11KeyboardHandler(this);
  initKeyboardHandler();
}

QVNCX11View::~QVNCX11View()
{
  delete gestureHandler;
}

/*!
    \reimp
*/
bool QVNCX11View::event(QEvent* e)
{
  switch (e->type()) {
  case QEvent::KeyboardLayoutChange:
    break;
  case QEvent::WindowBlocked:
    //      if (hwnd_)
    //        EnableWindow(hwnd_, false);
    break;
  case QEvent::WindowUnblocked:
    //      if (hwnd_)
    //        EnableWindow(hwnd_, true);
    break;
  case QEvent::WindowActivate:
    // qDebug() << "WindowActivate";
    grabPointer();
    break;
  case QEvent::WindowDeactivate:
    // qDebug() << "WindowDeactivate";
    ungrabPointer();
    break;
    //    case QEvent::Enter:
    //      qDebug() << "Enter";
    //      grabPointer();
    //      break;
    //    case QEvent::Leave:
    //      qDebug() << "Leave";
    //      ungrabPointer();
    //      break;
  case QEvent::CursorChange:
    // qDebug() << "CursorChange";
    e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    return true;
  case QEvent::Gesture:
    gestureEvent(reinterpret_cast<QGestureEvent*>(e));
  default:
    // qDebug() << "Unprocessed Event: " << e->type();
    break;
  }
  return QWidget::event(e);
}

void QVNCX11View::bell()
{

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  auto display = QX11Info::display();
#else
  auto display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  XBell(display, 0 /* volume */);
}

void QVNCX11View::handleClipboardData(const char*) {}

bool QVNCX11View::gestureEvent(QGestureEvent* event)
{
  qDebug() << "QVNCX11View::gestureEvent: event=" << event;
  int eid = eventNumber++;
  for (QGesture*& gesture : event->gestures()) {
    QPoint hotspot(0, 0);
    if (gesture->hasHotSpot()) {
      hotspot = mapFromGlobal(gesture->hotSpot().toPoint());
    }
    switch (gesture->state()) {
    case Qt::GestureStarted:
      gestureHandler->handleTouchBegin(eid, hotspot.x(), hotspot.y());
      break;
    case Qt::GestureUpdated:
      gestureHandler->handleTouchUpdate(eid, hotspot.x(), hotspot.y());
      break;
    case Qt::GestureFinished:
      gestureHandler->handleTouchEnd(eid);
      break;
    default:
      break;
    }
  }
  return true;
}
