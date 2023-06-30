#include <QTimer>
#include <QBitmap>
#include <QPainter>
#include "parameters.h"
#include "i18n.h"
#include "vnctoast.h"
#undef asprintf

#if defined(WIN32)
#include <windows.h>
#endif

QVNCToast::QVNCToast(QWidget *parent)
 : QLabel(QString::asprintf(_("Press %s to open the context menu"), ViewerConfig::config()->menuKey().toStdString().c_str()), parent, Qt::Widget)
 , closeTimer_(new QTimer)
{
  int radius = 5;
  hide();
  setWindowModality(Qt::NonModal);
#if defined(WIN32)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  setAttribute(Qt::WA_NoBackground);
#endif
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_DeleteOnClose);
  setAttribute(Qt::WA_TranslucentBackground);
#endif

  setGeometry(0, 0, 300, 40);
  setStyleSheet(QString("QLabel {"
                        "border-radius: %1px;"
                        "background-color: #96101010;"
                        "color: #e0ffffff;"
                        "font-size: 14px;"
                        "font-weight: bold;"
                        "}").arg(radius)); // NOTE: Alpha is effective only on macOS.
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

#if defined(WIN32)
  HWND hwnd = HWND(winId());
  LONG lStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
  lStyle |= WS_EX_LAYERED;
  SetWindowLong(hwnd, GWL_EXSTYLE, lStyle);
  SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 180, LWA_ALPHA);
#endif

  closeTimer_->setInterval(5000);
  closeTimer_->setSingleShot(true);
  connect(closeTimer_, &QTimer::timeout, this, &QVNCToast::hide);
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
