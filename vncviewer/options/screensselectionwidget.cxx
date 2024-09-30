#include "screensselectionwidget.h"

#include "../parameters.h"
#include "../viewerconfig.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
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

  void paintEvent(QPaintEvent* /*event*/) override
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

    if (property("included").toBool()) {
      p.setPen(Qt::NoPen);

      QColor color1 = "#ffafafe7";
      QColor color2 = "#ff5454ff";

      if (!isEnabled()) {
        color1 = color1.lighter(120);
        color2 = "#ffafafe7";
      }

      int sz = 5;
      for(int i = 0; i < width(); i += sz) {
        for(int j = 0; j < height(); j += sz){
          p.setBrush(QColor((((i+j) / sz) % 2) ? color1 : color2));
          p.drawRect(QRect(i+w, j+w, sz, sz));
        }
      }

      p.setPen(pen);
      p.setBrush(Qt::NoBrush);
      p.drawRect(rect().adjusted(w, w, -w, -w));
    }
  }
};

ScreensSelectionWidget::ScreensSelectionWidget(QWidget* parent)
  : QWidget{parent}
{
  setMinimumSize(200, 100);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  exclusiveButtons = new QButtonGroup(this);

  QList<QScreen*> screens = qApp->screens();
  for(auto& screen : screens) {
    connect(screen, &QScreen::geometryChanged, this, &ScreensSelectionWidget::moveCheckBoxes);
    connect(screen, &QScreen::virtualGeometryChanged, this, &ScreensSelectionWidget::moveCheckBoxes);
  }
  connect(qApp, &QGuiApplication::screenAdded, this, [=](){ hide(); reset(); show(); });
  connect(qApp, &QGuiApplication::screenRemoved, this, [=](){ hide(); reset(); show(); });
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

    int lw = rw * width();
    int lh = rh * height();
    int lx = rx * width();
    int ly = ry * height();

    CheckBox* newCheckBox = new CheckBox(this);
    newCheckBox->resize(lw, lh);
    newCheckBox->move(lx, ly);
    newCheckBox->setProperty("screenIndex", screenIndex);
    if (::fullScreenSelectedMonitors.getParam().count(screenIndex + 1))
      newCheckBox->setChecked(true);
    connect(newCheckBox, &QCheckBox::clicked, this, [=](bool checked) {
      newCheckBox->setProperty("included", false);
      newCheckBox->repaint();

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
          newCheckBox->repaint();
        }
      }

      moveCheckBoxes();
    });

    checkBoxes.append(newCheckBox);
    if (!ViewerConfig::canFullScreenOnMultiDisplays()) {
      exclusiveButtons->addButton(newCheckBox);
    }
  }
}

void ScreensSelectionWidget::showEvent(QShowEvent* event)
{
  moveCheckBoxes();
  QWidget::showEvent(event);
}

void ScreensSelectionWidget::resizeEvent(QResizeEvent* event)
{
  moveCheckBoxes();
  QWidget::resizeEvent(event);
}

void ScreensSelectionWidget::moveCheckBoxes()
{
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

  selectedRect = QRect();

  for (int& screenIndex : availableScreens) {
    QScreen* screen = screens[screenIndex];
    qreal rx = (screen->geometry().x() - xmin);
    qreal ry = (screen->geometry().y() - ymin);
    qreal rw = screen->geometry().width();
    qreal rh = screen->geometry().height();
    int lw = rw * ratio;
    int lh = rh * ratio;
    int lx = rx * ratio;
    int ly = ry * ratio;

    auto it = std::find_if(checkBoxes.begin(), checkBoxes.end(), [=](QCheckBox* const& c) {
      return c->property("screenIndex") == screenIndex;
    });
    if (it != checkBoxes.end()) {
      (*it)->resize(lw, lh);
      (*it)->move(lx, ly);
      if ((*it)->isChecked()) {
        selectedRect = selectedRect.united((*it)->geometry());
      }
    }
  }

  updatePartiallyChecked();
}

void ScreensSelectionWidget::updatePartiallyChecked()
{
  if (!ViewerConfig::canFullScreenOnMultiDisplays()) {
    return;
  }

  for (auto& checkBox : checkBoxes) {
    if (checkBox->property("included").toBool()) {
      checkBox->setProperty("included", false);
      checkBox->repaint();
    }

    if (!checkBox->isChecked()) {
      if (selectedRect.contains(checkBox->geometry())) {
        checkBox->setProperty("included", true);
        checkBox->repaint();
      }
    }
  }
}
