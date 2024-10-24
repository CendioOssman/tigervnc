#include "toast.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <rfb/util.h>

#include "parameters.h"
#include "i18n.h"

#include <QTimer>
#include <QPainter>

Toast::Toast(QWidget* parent)
  : QWidget{parent}
  , toastTimer(new QTimer(this))
{
  hide();

  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  // FIXME: This prevents rendering of lower widgets on Qt6 on X11
  //setAttribute(Qt::WA_TranslucentBackground);
  // FIXME: These two are for windows, not widgets. Are they needed?
  setWindowFlag(Qt::WindowTransparentForInput, true);
  setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);

  toastTimer->setInterval(5000);
  toastTimer->setSingleShot(true);
  connect(toastTimer, &QTimer::timeout, this, &Toast::hideToast);
}

void Toast::showToast()
{
  toastTimer->start();
  show();
  raise();
}

void Toast::hideToast()
{
  toastTimer->stop();
  hide();
}

QFont Toast::toastFont() const
{
  QFont f;
  f.setBold(true);
  f.setPixelSize(14);
  return f;
}

QString Toast::toastText() const
{
  return rfb::format(_("Press %s to open the context menu"), ::menuKey.getValueStr().c_str()).c_str();
}

QRect Toast::toastGeometry() const
{
  QFontMetrics fm(toastFont());
  int b = 8;
  QRect r = fm.boundingRect(toastText()).adjusted(-2 * b, -2 * b, 2 * b, 2 * b);

  int x = (width() - r.width()) / 2;
  int y = 50;
  return QRect(QPoint(x, y), r.size());
}

void Toast::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.setFont(toastFont());
  painter.setPen(Qt::NoPen);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor("#96101010"));
  painter.drawRoundedRect(toastGeometry(), 15, 15, Qt::AbsoluteSize);
  QPen p;
  p.setColor("#e0ffffff");
  painter.setPen(p);
  painter.drawText(toastGeometry(), toastText(), QTextOption(Qt::AlignCenter));
}
