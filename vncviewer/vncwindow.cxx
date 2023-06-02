#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QMoveEvent>
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
  setWidgetResizable(ViewerConfig::config()->remoteResize());
  setContentsMargins(0, 0, 0, 0);
  setFrameStyle(QFrame::NoFrame);

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
  QPoint point = mapToGlobal(QPoint(0, 0));
  toast_->move(point.x() + (width() - toast_->width()) / 2, point.y() + 50);
  toast_->show();
}

void QVNCWindow::moveEvent(QMoveEvent *e)
{
  QWidget::moveEvent(e);
  if (toast_->isVisible()) {
    QPoint point = mapToGlobal(QPoint(0, 0));
    toast_->move(point.x() + (width() - toast_->width()) / 2, point.y() + 50);
  }
}
