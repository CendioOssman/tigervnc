#include "alertdialog.h"
#include "parameters.h"
#include "appmanager.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

AlertDialog::AlertDialog(QString message, bool quit, QWidget *parent)
    : QDialog{parent}
{
    setWindowTitle(tr("TigerVNC Viewer"));

    QVBoxLayout* layout = new QVBoxLayout;
    QLabel* label = new QLabel(message);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label, 1);

    QHBoxLayout* btnsLayout = new QHBoxLayout;
    btnsLayout->addStretch(1);
    if(ViewerConfig::config()->reconnectOnError() && !quit)
    {
        QPushButton* reconnectBtn = new QPushButton(tr("Reconnect"));
        btnsLayout->addWidget(reconnectBtn, 0, Qt::AlignRight);
        connect(reconnectBtn, &QPushButton::clicked, this, [=](){
            AppManager::instance()->connectToServer("");
            close();
        });
    }
    QPushButton* closeBtn = new QPushButton(tr("Close"));
    btnsLayout->addWidget(closeBtn, 0, Qt::AlignRight);
    connect(closeBtn, &QPushButton::clicked, this, [=](){
        if (quit) {
            qApp->quit();
        }
        close();
    });
    layout->addLayout(btnsLayout);

    setLayout(layout);
}
