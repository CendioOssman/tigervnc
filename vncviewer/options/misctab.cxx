#include "misctab.h"
#include "parameters.h"

#include <QVBoxLayout>
#include <QCheckBox>

MiscTab::MiscTab(QWidget *parent)
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
    ViewerConfig::config()->setShared(shared->isChecked());
    ViewerConfig::config()->setReconnectOnError(reconnect->isChecked());
}

void MiscTab::reset()
{
    shared->setChecked(ViewerConfig::config()->shared());
    reconnect->setChecked(ViewerConfig::config()->reconnectOnError());
}
