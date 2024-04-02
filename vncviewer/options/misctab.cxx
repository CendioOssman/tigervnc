#include "misctab.h"

#include "parameters.h"

#include <QCheckBox>
#include <QVBoxLayout>

MiscTab::MiscTab(QWidget* parent)
  : TabElement{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;
  shared = new QCheckBox(tr("Shared (don't disconnect other viewers)"));
  layout->addWidget(shared);
  reconnect = new QCheckBox(tr("Ask to reconnect on connection errors"));
  layout->addWidget(reconnect);
  layout->addStretch(1);
  setLayout(layout);
}

void MiscTab::apply()
{
  ::shared.setParam(shared->isChecked());
  ::reconnectOnError.setParam(reconnect->isChecked());
}

void MiscTab::reset()
{
  shared->setChecked(::shared);
  reconnect->setChecked(::reconnectOnError);
}
