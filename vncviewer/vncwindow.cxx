#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QMoveEvent>
#include <QResizeEvent>
#include <QHBoxLayout>
#include <QGestureEvent>
#include <QPainter>
//#include <QCoreApplication>
#include <QDebug>
#include "rfb/LogWriter.h"
#include "parameters.h"
#include "i18n.h"
#include "vnctoast.h"
#include "vncwindow.h"
#undef asprintf

static rfb::LogWriter vlog("VNCWindow");

QVNCWindow::QVNCWindow(QWidget *parent)
 : QScrollArea(parent)
 , toast_(new QVNCToast(this))
{
  setAttribute(Qt::WA_InputMethodTransparent);
  setAttribute(Qt::WA_NativeWindow);
  setFocusPolicy(Qt::StrongFocus);

  setWidgetResizable(ViewerConfig::config()->remoteResize());
  setContentsMargins(0, 0, 0, 0);
  setFrameRect(QRect(0, 0, 0, 0));
  setFrameStyle(QFrame::NoFrame);

  setLineWidth(0);
  setViewportMargins(0, 0, 0, 0);

  QPalette p(palette());
  p.setColor(QPalette::Window, QColor::fromRgb(0, 0, 0));
  setPalette(p);
  setBackgroundRole(QPalette::Window);

  setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  // Support for -geometry option. Note that although we do support
  // negative coordinates, we do not support -XOFF-YOFF (ie
  // coordinates relative to the right edge / bottom edge) at this
  // time.
  int geom_x = 0, geom_y = 0;
  if (!ViewerConfig::config()->geometry().isEmpty()) {
    int nfields = sscanf((const char*)ViewerConfig::config()->geometry().toStdString().c_str(), "+%d+%d", &geom_x, &geom_y);
    if (nfields != 2) {
      int geom_w, geom_h;
      nfields = sscanf((const char*)ViewerConfig::config()->geometry().toStdString().c_str(), "%dx%d+%d+%d", &geom_w, &geom_h, &geom_x, &geom_y);
      if (nfields != 4) {
        vlog.error(_("Invalid geometry specified!"));
      }
    }
    if (nfields == 2 || nfields == 4) {
      move(geom_x, geom_y);
    }
  }
}

QVNCWindow::~QVNCWindow()
{
  delete toast_;
}

void QVNCWindow::popupToast()
{
  toast_->move((width() - toast_->width()) / 2, 50);
  toast_->show();
  toast_->raise();
}

void QVNCWindow::moveEvent(QMoveEvent *e)
{
  QWidget::moveEvent(e);
}

void QVNCWindow::resizeEvent(QResizeEvent *e)
{
  if (ViewerConfig::config()->remoteResize()) {
    QSize size = e->size();
    widget()->resize(size.width(), size.height());
  }
}

void QVNCWindow::changeEvent(QEvent *e)
{
  if (e->type() == QEvent::WindowStateChange) {
    if (ViewerConfig::config()->remoteResize()) {
      qDebug() << "QVNCWindow::changeEvent: w=" << width() << ",h=" << height() << ",state=" << windowState() << ",oldState=" << (static_cast<QWindowStateChangeEvent*>(e))->oldState();
      widget()->resize(width(), height());
    }
  }
}

void QVNCWindow::resize(int width, int height)
{
  QScrollArea::resize(width, height);
  if (widgetResizable()) {
#if !defined(__APPLE__)
    double dpr = devicePixelRatioF();
    width *= dpr;
    height *= dpr;
#endif
    widget()->resize(width, height);
  }
}

void QVNCWindow::normalizedResize(int width, int height)
{
#if !defined(__APPLE__)
  double dpr = devicePixelRatioF();
  width /= dpr;
  height /= dpr;
#endif
  QScrollArea::resize(width, height);
}
