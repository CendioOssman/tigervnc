#include "optionsdialog.h"
#include "options/compressiontab.h"
#include "options/displaytab.h"
#include "options/inputtab.h"
#include "options/misctab.h"
#include "options/securitytab.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>

OptionsDialog::OptionsDialog(QWidget *parent)
    : QDialog{parent}
{
    setWindowTitle(tr("TigerVNC Options"));

    QVBoxLayout* layout = new QVBoxLayout;

    tabWidget = new QTabWidget;
    tabWidget->addTab(new CompressionTab, tr("Compression"));
    tabWidget->addTab(new SecurityTab, tr("Security"));
    tabWidget->addTab(new InputTab, tr("Input"));
    tabWidget->addTab(new DisplayTab, tr("Display"));
    tabWidget->addTab(new MiscTab, tr("Misc"));

    layout->addWidget(tabWidget);

    QHBoxLayout* btnsLayout = new QHBoxLayout;
    btnsLayout->addStretch(1);
    QPushButton* applyBtn = new QPushButton(tr("Apply"));
    btnsLayout->addWidget(applyBtn, 0, Qt::AlignRight);
    QPushButton* closeBtn = new QPushButton(tr("Close"));
    btnsLayout->addWidget(closeBtn, 0, Qt::AlignRight);
    layout->addLayout(btnsLayout);

    setLayout(layout);

    setMinimumSize(1024, 600);

    reset();

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    connect(applyBtn, &QPushButton::clicked, this, &OptionsDialog::apply);
}

void OptionsDialog::apply()
{
    for(int i = 0; i < tabWidget->count(); ++i)
    {
        auto w = qobject_cast<TabElement*>(tabWidget->widget(i));
        if(w) {
            w->apply();
        }
    }
}

void OptionsDialog::reset()
{
    for(int i = 0; i < tabWidget->count(); ++i)
    {
        auto w = qobject_cast<TabElement*>(tabWidget->widget(i));
        if(w) {
            w->reset();
        }
    }
}
