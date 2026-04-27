#include "QMonitorArrangement.h"

#include "parameters.h"

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

QMonitorArrangement::QMonitorArrangement(QWidget* parent)
  : QWidget{parent}
{
  setMinimumSize(200, 100);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

  QList<QScreen*> screens = qApp->screens();
  for(auto& screen : screens) {
    connect(screen, &QScreen::geometryChanged, this, &QMonitorArrangement::moveCheckBoxes);
    connect(screen, &QScreen::virtualGeometryChanged, this, &QMonitorArrangement::moveCheckBoxes);
  }
  connect(qApp, &QGuiApplication::screenAdded, this, [=](){ hide(); reset(); show(); });
  connect(qApp, &QGuiApplication::screenRemoved, this, [=](){ hide(); reset(); show(); });
}

void QMonitorArrangement::apply()
{
  QList<QScreen*> screens = qApp->screens();
  std::set<QScreen*> selectedScreens;
  for (auto const& c : qAsConst(checkBoxes)) {
    if (c->isChecked()) {
      QRect geometry = c->property("screenGeometry").toRect();
      for (QScreen* screen : screens) {
        if (screen->geometry() == geometry) {
          selectedScreens.insert(screen);
          break;
        }
      }
    }
  }
  ::fullScreenSelectedMonitors.setParam(selectedScreens);
}

void QMonitorArrangement::reset()
{
  qDeleteAll(checkBoxes);
  checkBoxes.clear();

  QRect virtualGeometry = qApp->primaryScreen()->virtualGeometry();
  QList<QScreen*> screens = qApp->screens();
  std::set<QScreen*> configScreens = fullScreenSelectedMonitors.getParam();

  for (QScreen* screen : screens) {
    float rx = (screen->geometry().x() - virtualGeometry.x()) /
               virtualGeometry.width();
    float ry = (screen->geometry().y() - virtualGeometry.y()) /
               virtualGeometry.height();
    float rw = screen->geometry().width() / virtualGeometry.width();
    float rh = screen->geometry().height() / virtualGeometry.height();

    int lw = rw * width();
    int lh = rh * height();
    int lx = rx * width();
    int ly = ry * height();

    CheckBox* newCheckBox = new CheckBox(this);
    newCheckBox->resize(lw, lh);
    newCheckBox->move(lx, ly);
    newCheckBox->setProperty("screenGeometry", screen->geometry());
    if (std::find_if(configScreens.begin(), configScreens.end(),
                     [screen](QScreen* s) {
                       return s->geometry() == screen->geometry();
                     }) != configScreens.end())
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
  }
}

void QMonitorArrangement::showEvent(QShowEvent* event)
{
  moveCheckBoxes();
  QWidget::showEvent(event);
}

void QMonitorArrangement::resizeEvent(QResizeEvent* event)
{
  moveCheckBoxes();
  QWidget::resizeEvent(event);
}

void QMonitorArrangement::moveCheckBoxes()
{
  QRect virtualGeometry = qApp->primaryScreen()->virtualGeometry();
  QList<QScreen*> screens = qApp->screens();
  float ratio = qMin(((float)width() / virtualGeometry.width()),
                     ((float)height() / virtualGeometry.height()));

  selectedRect = QRect();

  for (QScreen* screen : screens) {
    float rx = (screen->geometry().x() - virtualGeometry.x());
    float ry = (screen->geometry().y() - virtualGeometry.y());
    float rw = screen->geometry().width();
    float rh = screen->geometry().height();
    int lw = rw * ratio;
    int lh = rh * ratio;
    int lx = rx * ratio;
    int ly = ry * ratio;

    for (QCheckBox* c : checkBoxes) {
      if (c->property("screenGeometry") != screen->geometry())
        continue;

      c->resize(lw, lh);
      c->move(lx, ly);
      if (c->isChecked())
        selectedRect = selectedRect.united(c->geometry());

      break;
    }
  }

  updatePartiallyChecked();
}

void QMonitorArrangement::updatePartiallyChecked()
{
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
