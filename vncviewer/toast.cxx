#include "toast.h"

#include "parameters.h"
#include "vncconnection.h"
#include "i18n.h"

#include <QTimer>
#include <QDebug>
#include <QPainter>

Toast::Toast(QWidget* parent)
  : QWidget{parent}
  , toastTimer(new QTimer(this))
{
  hide();

  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);

  toastTimer->setInterval(5000);
  toastTimer->setSingleShot(true);
  connect(toastTimer, &QTimer::timeout, this, &Toast::hideToast);
}

void Toast::showToast()
{
  toastTimer->start();
  show();
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
  return QString::asprintf(_("Press %s to open the context menu"), ::menuKey.getValueStr().c_str());
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

void Toast::paintEvent(QPaintEvent *event)
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
