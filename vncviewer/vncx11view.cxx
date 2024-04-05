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
#include "X11KeyboardHandler.h"
#include "appmanager.h"
#include "rfb/LogWriter.h"
#include "vncgesturerecognizer.h"
#include "vncx11view.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xrender.h>

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

bool QVNCX11View::event(QEvent* e)
{
  switch (e->type()) {
  case QEvent::Gesture:
    gestureEvent(reinterpret_cast<QGestureEvent*>(e));
    break;
  default:
    break;
  }
  return QAbstractVNCView::event(e);
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

bool QVNCX11View::gestureEvent(QGestureEvent* event)
{
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
