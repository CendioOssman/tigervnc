/* Copyright 2022-2026 Pierre Ossman for Cendio AB
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

#include <QHBoxLayout>
#include <QListWidget>
#include <QProxyStyle>
#include <QStackedWidget>
#include <QStyledItemDelegate>

#include "QNavigation.h"

#if !defined(WIN32) && !defined(__APPLE__)
class ListViewStyle : public QProxyStyle
{
public:
  void drawPrimitive(PrimitiveElement element, const QStyleOption * option, QPainter * painter, const QWidget * widget = nullptr) const override
  {
    if (PE_FrameFocusRect == element) {
      return;
    }

    if (PE_PanelItemViewItem == element) {
      QStyleOptionViewItem newOption = *qstyleoption_cast<const QStyleOptionViewItem *>(option);
      newOption.showDecorationSelected = true;
      QProxyStyle::drawPrimitive(element, &newOption, painter, widget);
      return;
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
  }
};
#endif

class OptionsDelegate : public QStyledItemDelegate
{
public:
  explicit OptionsDelegate(QObject *parent = nullptr)
    : QStyledItemDelegate(parent)
  {

  }

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
  {
    int width = option.fontMetrics.boundingRect(index.data().toString()).width();
    return QSize(width + 40, 40);
  }
};

QNavigation::QNavigation(QWidget* parent)
  : QFrame(parent)
{
  setFrameShape(QFrame::StyledPanel);

  QBoxLayout* layout = new QHBoxLayout;

  listWidget = new QListWidget;
#if !defined(WIN32) && !defined(__APPLE__)
  listWidget->setStyle(new ListViewStyle);
#endif
  listWidget->setFrameShape(QFrame::NoFrame);
  listWidget->setItemDelegate(new OptionsDelegate(this));
  layout->addWidget(listWidget);

  QFrame* divider = new QFrame;
  divider->setFrameShape(QFrame::StyledPanel);
  divider->setFixedWidth(1);
  layout->addWidget(divider);

  tabWidget = new QStackedWidget;
  layout->addWidget(tabWidget, 1);

  connect(listWidget, &QListWidget::currentRowChanged,
          tabWidget, &QStackedWidget::setCurrentIndex);

  setLayout(layout);
}

int QNavigation::addWidget(const QString& title, QWidget* widget)
{
  return insertWidget(count(), title, widget);
}

int QNavigation::insertWidget(int index, const QString& title,
                              QWidget* widget)
{
  int newIndex;

  listWidget->insertItem(index, title);
  listWidget->setFixedWidth(listWidget->sizeHintForColumn(0));
  newIndex = tabWidget->insertWidget(index, widget);

  if (count() == 1)
    listWidget->setCurrentRow(0);

  return newIndex;
}

void QNavigation::removeWidget(QWidget* widget)
{
  int index = indexOf(widget);
  delete listWidget->takeItem(index);
  listWidget->setFixedWidth(listWidget->sizeHintForColumn(0));
  tabWidget->removeWidget(widget);
}

int QNavigation::currentIndex() const
{
  return tabWidget->currentIndex();
}

QWidget* QNavigation::currentWidget() const
{
  return tabWidget->currentWidget();
}

void QNavigation::setCurrentIndex(int index)
{
  listWidget->setCurrentRow(index);
}

void QNavigation::setCurrentWidget(QWidget* widget)
{
  int index;
  index = indexOf(widget);
  if (index == -1)
    return;
  setCurrentIndex(index);
}

int QNavigation::count() const
{
  return tabWidget->count();
}

int QNavigation::indexOf(QWidget* widget) const
{
  return tabWidget->indexOf(widget);
}

QWidget* QNavigation::widget(int index) const
{
  return tabWidget->widget(index);
}
