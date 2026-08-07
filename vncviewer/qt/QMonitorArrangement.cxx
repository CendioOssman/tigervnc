/* Copyright 2021 Hugo Lundin <huglu@cendio.se> for Cendio AB.
 * Copyright 2021-2026 Pierre Ossman for Cendio AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QVBoxLayout>

#include "QMonitorArrangement.h"

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
      for (int i = 0; i < width(); i += sz) {
        for (int j = 0; j < height(); j += sz) {
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
  : QWidget(parent)
{
  setMinimumSize(200, 100);
  setSizePolicy(QSizePolicy::MinimumExpanding,
                QSizePolicy::MinimumExpanding);

  QList<QScreen*> screens = qApp->screens();
  for (QScreen* screen : screens) {
    connect(screen, &QScreen::geometryChanged, this,
            &QMonitorArrangement::moveCheckBoxes);
    connect(screen, &QScreen::virtualGeometryChanged, this,
            &QMonitorArrangement::moveCheckBoxes);
  }
  connect(qApp, &QGuiApplication::screenAdded,
          this, &QMonitorArrangement::refresh);
  connect(qApp, &QGuiApplication::screenRemoved,
          this, &QMonitorArrangement::refresh);
}

QList<QScreen*> QMonitorArrangement::screens()
{
  QList<QScreen*> allScreens = qApp->screens();
  QList<QScreen*> selectedScreens;

  for (QCheckBox* check : checkBoxes) {
    QRect geometry;

    if (!check->isChecked())
      continue;

    geometry = check->property("screenGeometry").toRect();
    for (QScreen* screen : allScreens) {
      if (screen->geometry() == geometry) {
        selectedScreens.push_back(screen);
        break;
      }
    }
  }

  return selectedScreens;
}

void QMonitorArrangement::setScreens(const QList<QScreen*>& screens)
{
  qDeleteAll(checkBoxes);
  checkBoxes.clear();

  QRect virtualGeometry = qApp->primaryScreen()->virtualGeometry();
  QList<QScreen*> allScreens = qApp->screens();

  for (QScreen* screen : allScreens) {
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
    if (std::find_if(screens.begin(), screens.end(),
                     [screen](QScreen* s) {
                       return s->geometry() == screen->geometry();
                     }) != screens.end())
      newCheckBox->setChecked(true);
    connect(newCheckBox, &QCheckBox::clicked, this,
            [this, newCheckBox](bool checked) {
              newCheckBox->setProperty("included", false);
              newCheckBox->repaint();

              if (!checked) {
                bool noChecked = true;
                for (QCheckBox* c : checkBoxes) {
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
  for (QCheckBox* checkBox : checkBoxes) {
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

void QMonitorArrangement::refresh()
{
  hide();
  setScreens(screens());
  show();
}
