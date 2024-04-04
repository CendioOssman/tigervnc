#include "optionsdialog.h"

#include "options/compressiontab.h"
#include "options/displaytab.h"
#include "options/inputtab.h"
#include "options/misctab.h"
#include "options/securitytab.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

OptionsDialog::OptionsDialog(bool staysOnTop, QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(tr("TigerVNC Options"));
  setWindowFlag(Qt::WindowStaysOnTopHint, staysOnTop);

  QVBoxLayout* layout = new QVBoxLayout;

  QHBoxLayout* hLayout = new QHBoxLayout;

  QListWidget* listWidget = new QListWidget;
  QStringList tabs = {tr("Compression"), tr("Security"), tr("Input"), tr("Display"), tr("Misc")};
  listWidget->addItems(tabs);
  listWidget->setCurrentRow(0);

  hLayout->addWidget(listWidget);

  tabWidget = new QStackedWidget;
  tabWidget->addWidget(new CompressionTab);
  tabWidget->addWidget(new SecurityTab);
  tabWidget->addWidget(new InputTab);
  tabWidget->addWidget(new DisplayTab);
  tabWidget->addWidget(new MiscTab);

  hLayout->addWidget(tabWidget, 1);

  connect(listWidget, &QListWidget::currentRowChanged, tabWidget, &QStackedWidget::setCurrentIndex);

  layout->addLayout(hLayout);

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->addStretch(1);
  QPushButton* applyBtn = new QPushButton(tr("Apply"));
  btnsLayout->addWidget(applyBtn, 0, Qt::AlignRight);
  QPushButton* closeBtn = new QPushButton(tr("Close"));
  btnsLayout->addWidget(closeBtn, 0, Qt::AlignRight);
  layout->addLayout(btnsLayout);

  setLayout(layout);

  setMinimumSize(600, 600);

  reset();

  connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
  connect(applyBtn, &QPushButton::clicked, this, &OptionsDialog::apply);
}

void OptionsDialog::apply()
{
  for (int i = 0; i < tabWidget->count(); ++i) {
    auto w = qobject_cast<TabElement*>(tabWidget->widget(i));
    if (w) {
      w->apply();
    }
  }
  close();
}

void OptionsDialog::reset()
{
  for (int i = 0; i < tabWidget->count(); ++i) {
    auto w = qobject_cast<TabElement*>(tabWidget->widget(i));
    if (w) {
      w->reset();
    }
  }
}
