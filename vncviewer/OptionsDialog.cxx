/* Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "OptionsDialog.h"

#include "OptionsCompression.h"
#include "OptionsDisplay.h"
#include "OptionsInput.h"
#include "OptionsMisc.h"
#include "OptionsSecurity.h"
#include "i18n.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QProxyStyle>
#include <QTimer>

std::map<OptionsCallback*, void*> OptionsDialog::callbacks;

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
      auto newOption = *qstyleoption_cast<const QStyleOptionViewItem *>(option);
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

OptionsDialog::OptionsDialog(bool staysOnTop, QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(_("VNC Viewer: Connection Options"));
  setWindowFlag(Qt::WindowStaysOnTopHint, staysOnTop);

  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);

  QHBoxLayout* hLayout = new QHBoxLayout;

  QListWidget* listWidget = new QListWidget;
#if !defined(WIN32) && !defined(__APPLE__)
  listWidget->setStyle(new ListViewStyle);
#endif
  listWidget->setFrameShape(QFrame::NoFrame);
  listWidget->setItemDelegate(new OptionsDelegate(this));
  QStringList tabs = {"   " + QString(_("Compression")),
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
                      "   " + QString(_("Security")),
#endif
                      "   " + QString(_("Input")),
                      "   " + QString(_("Display")),
                      "   " + QString(_("Misc"))};
  listWidget->addItems(tabs);
  listWidget->setCurrentRow(0);
  listWidget->setFixedWidth(listWidget->sizeHintForColumn(0));

  hLayout->addWidget(listWidget);

  QFrame* vFrame = new QFrame;
  vFrame->setFrameShape(QFrame::StyledPanel);
  vFrame->setFixedWidth(1);
  hLayout->addWidget(vFrame);

  tabWidget = new QStackedWidget;
  tabWidget->addWidget(new OptionsCompression);
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
  tabWidget->addWidget(new OptionsSecurity);
#endif
  tabWidget->addWidget(new OptionsInput);
  tabWidget->addWidget(new OptionsDisplay);
  tabWidget->addWidget(new OptionsMisc);

  hLayout->addWidget(tabWidget, 1);

  connect(listWidget, &QListWidget::currentRowChanged, tabWidget, &QStackedWidget::setCurrentIndex);

  layout->addLayout(hLayout);

  QFrame* hFrame = new QFrame;
  hFrame->setFrameShape(QFrame::StyledPanel);
  hFrame->setFixedHeight(1);
  layout->addWidget(hFrame);

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->setContentsMargins(10,10,10,10);

  QDialogButtonBox* buttonBox = new QDialogButtonBox;
  buttonBox->addButton(QDialogButtonBox::Ok);
  buttonBox->addButton(QDialogButtonBox::Cancel);
  connect(buttonBox, &QDialogButtonBox::accepted,
          this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected,
          this, &QDialog::reject);
  btnsLayout->addWidget(buttonBox);

  layout->addLayout(btnsLayout);

  setLayout(layout);
  adjustSize();

  reset();

  connect(this, &QDialog::accepted, this, &OptionsDialog::apply);
}

void OptionsDialog::apply()
{
  for (int i = 0; i < tabWidget->count(); ++i) {
    auto w = qobject_cast<OptionsPage*>(tabWidget->widget(i));
    if (w) {
      w->apply();
    }
  }

  std::map<OptionsCallback*, void*>::const_iterator iter;

  for (iter = callbacks.begin();iter != callbacks.end();++iter)
    iter->first(iter->second);

  close();
}

void OptionsDialog::reset()
{
  for (int i = 0; i < tabWidget->count(); ++i) {
    auto w = qobject_cast<OptionsPage*>(tabWidget->widget(i));
    if (w) {
      w->reset();
    }
  }
}

void OptionsDialog::addCallback(OptionsCallback *cb, void *data)
{
  callbacks[cb] = data;
}


void OptionsDialog::removeCallback(OptionsCallback *cb)
{
  callbacks.erase(cb);
}
