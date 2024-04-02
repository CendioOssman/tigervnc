#include "screensselectionwidget.h"

#include "parameters.h"
#include "viewerconfig.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QVBoxLayout>

class CheckBox : public QCheckBox
{
public:
  CheckBox(QWidget* parent)
    : QCheckBox(parent)
  {
  }

protected:
  bool hitButton(const QPoint& pos) const override { return rect().contains(pos); }

  void paintEvent(QPaintEvent* event) override
  {
    QPainter p(this);

    if (isEnabled()) {
      if (isChecked())
        p.setBrush(QColor("#ff5454ff"));
      else
        p.setBrush(palette().brush(QPalette::Base));
    } else {
      if (isChecked())
        p.setBrush(QColor("#ffafafe7"));
      else
        p.setBrush(palette().brush(QPalette::Window));
    }

    QPen pen;
    int w = 1;
    pen.setWidth(w);
    p.setPen(pen);
    p.drawRect(rect().adjusted(w, w, -w, -w));
  }
};

ScreensSelectionWidget::ScreensSelectionWidget(QWidget* parent)
  : QWidget{parent}
{
  setMinimumSize(200, 100);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  exclusiveButtons = new QButtonGroup(this);
}

void ScreensSelectionWidget::getGlobalScreensGeometry(QList<int> screens, int& xmin, int& ymin, qreal& w, qreal& h)
{
  QList<QScreen*> appScreens = qApp->screens();
  xmin = INT_MAX;
  ymin = INT_MAX;
  int xmax = INT_MIN;
  int ymax = INT_MIN;
  for (int& screenIndex : screens) {
    QScreen* screen = appScreens[screenIndex];
    QRect rect = screen->geometry();
    qDebug() << screenIndex << rect;
    if (xmin > rect.x()) {
      xmin = rect.x();
    }
    if (xmax < rect.x() + rect.width()) {
      xmax = rect.x() + rect.width();
    }
    if (ymin > rect.y()) {
      ymin = rect.y();
    }
    if (ymax < rect.y() + rect.height()) {
      ymax = rect.y() + rect.height();
    }
  }
  qDebug() << "ScreensSelectionWidget::getGlobalScreensGeometry" << xmin << ymin << xmax << ymax;
  w = xmax - xmin;
  h = ymax - ymin;
}

void ScreensSelectionWidget::apply()
{
  std::set<int> selectedScreens;
  for (auto const& c : qAsConst(checkBoxes)) {
    if (c->isChecked()) {
      selectedScreens.insert(c->property("screenIndex").toInt() + 1);
    }
  }
  ::fullScreenSelectedMonitors.setParam(selectedScreens);
}

void ScreensSelectionWidget::reset()
{
  qDeleteAll(checkBoxes);
  checkBoxes.clear();

  QList<QScreen*> screens = qApp->screens();
  QList<int> availableScreens;
  for (int i = 0; i < screens.length(); i++) {
    availableScreens << i;
  }

  qDebug() << "ScreensSelectionWidget::reset" << availableScreens;

  int xmin = INT_MAX;
  int ymin = INT_MAX;
  qreal w = INT_MAX;
  qreal h = INT_MAX;
  getGlobalScreensGeometry(availableScreens, xmin, ymin, w, h);

  for (int& screenIndex : availableScreens) {
    QScreen* screen = screens[screenIndex];
    qreal rx = (screen->geometry().x() - xmin) / w;
    qreal ry = (screen->geometry().y() - ymin) / h;
    qreal rw = screen->geometry().width() / w;
    qreal rh = screen->geometry().height() / h;
    qDebug() << "screen[" << screenIndex << "] rx=" << rx << ", ry=" << ry << ", rw=" << rw << ", rh=" << rh;
    int lw = rw * width();
    int lh = rh * height();
    int lx = rx * width();
    int ly = ry * height();
    qDebug() << "screen[" << screenIndex << "] lx=" << lx << ", ly=" << ly << ", lw=" << lw << ", lh=" << lh;
    CheckBox* newCheckBox = new CheckBox(this);
    newCheckBox->resize(lw, lh);
    newCheckBox->move(lx, ly);
    newCheckBox->setProperty("screenIndex", screenIndex);
    if (::fullScreenSelectedMonitors.getParam().count(screenIndex + 1))
      newCheckBox->setChecked(true);
    connect(newCheckBox, &QCheckBox::clicked, this, [=](bool checked) {
      if (!checked) {
        bool noChecked = true;
        for (auto const& c : qAsConst(checkBoxes)) {
          if (c->isChecked()) {
            noChecked = false;
            break;
          }
        }
        if (noChecked) {
          // we cannot have no screen selected
          newCheckBox->setChecked(true);
        }
      }
    });

    checkBoxes.append(newCheckBox);
    if (!ViewerConfig::canFullScreenOnMultiDisplays()) {
      exclusiveButtons->addButton(newCheckBox);
    }
  }
}

void ScreensSelectionWidget::showEvent(QShowEvent* event)
{
  qDebug() << "ScreensSelectionWidget::showEvent";
  moveCheckBoxes();
  QWidget::showEvent(event);
}

void ScreensSelectionWidget::resizeEvent(QResizeEvent* event)
{
  qDebug() << "ScreensSelectionWidget::resizeEvent";
  moveCheckBoxes();
  QWidget::resizeEvent(event);
}

void ScreensSelectionWidget::moveCheckBoxes()
{
  qDebug() << "ScreensSelectionWidget::moveCheckBoxes" << geometry();
  QList<QScreen*> screens = qApp->screens();
  QList<int> availableScreens;
  for (int i = 0; i < screens.length(); i++) {
    availableScreens << i;
  }

  int xmin = INT_MAX;
  int ymin = INT_MAX;
  qreal w = INT_MAX;
  qreal h = INT_MAX;
  getGlobalScreensGeometry(availableScreens, xmin, ymin, w, h);
  qreal ratio = qMin((width() / w), (height() / h));
  qDebug() << "ratio" << (width() / w) << (height() / h);

  for (int& screenIndex : availableScreens) {
    QScreen* screen = screens[screenIndex];
    qreal rx = (screen->geometry().x() - xmin);
    qreal ry = (screen->geometry().y() - ymin);
    qreal rw = screen->geometry().width();
    qreal rh = screen->geometry().height();
    qDebug() << "screen[" << screenIndex << "] rx=" << rx << ", ry=" << ry << ", rw=" << rw << ", rh=" << rh;
    int lw = rw * ratio;
    int lh = rh * ratio;
    int lx = rx * ratio;
    int ly = ry * ratio;
    qDebug() << "screen[" << screenIndex << "] lx=" << lx << ", ly=" << ly << ", lw=" << lw << ", lh=" << lh;

    auto it = std::find_if(checkBoxes.begin(), checkBoxes.end(), [=](QCheckBox* const& c) {
      return c->property("screenIndex") == screenIndex;
    });
    if (it != checkBoxes.end()) {
      (*it)->resize(lw, lh);
      (*it)->move(lx, ly);
    }
  }
}
