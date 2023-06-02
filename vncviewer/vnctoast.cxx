#include <QTimer>
#include <QBitmap>
#include <QPainter>
#include "parameters.h"
#include "i18n.h"
#include "vnctoast.h"
#undef asprintf

QVNCToast::QVNCToast(QWidget *parent)
 : QLabel(QString::asprintf(_("Press %s to open the context menu"), ViewerConfig::config()->menuKey().toStdString().c_str()), parent, Qt::SplashScreen | Qt::WindowStaysOnTopHint)
 , closeTimer_(new QTimer)
{
  int radius = 5;
  hide();
  setWindowModality(Qt::NonModal);
  setGeometry(0, 0, 300, 40);
  setStyleSheet(QString("QLabel {"
                        "border-radius: %1px;"
                        "background-color: #50505050;"
                        "color: #e0ffffff;"
                        "font-size: 14px;"
                        "font-weight: bold;"
                        "}").arg(radius));
  setWindowOpacity(0.8);
  const QRect rect(QPoint(0,0), geometry().size());
  QBitmap b(rect.size());
  b.fill(QColor(Qt::color0));
  QPainter painter(&b);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(Qt::color1);
  painter.drawRoundedRect(rect, radius, radius, Qt::AbsoluteSize);
  painter.end();
  setMask(b);
  setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  closeTimer_->setInterval(5000);
  closeTimer_->setSingleShot(true);
  connect(closeTimer_, &QTimer::timeout, this, &QWidget::hide);
}

QVNCToast::~QVNCToast()
{
  delete closeTimer_;
}

void QVNCToast::show()
{
  closeTimer_->start();
  QLabel::show();
}
