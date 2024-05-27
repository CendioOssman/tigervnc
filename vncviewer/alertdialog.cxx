#include "alertdialog.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "appmanager.h"
#include "viewerconfig.h"
#include "parameters.h"
#include "i18n.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AlertDialog::AlertDialog(bool staysOnTop, QString message, bool quit, bool reconnect, QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(_("TigerVNC Viewer"));
  setWindowFlag(Qt::WindowStaysOnTopHint, staysOnTop);

  QVBoxLayout* layout = new QVBoxLayout;
  QLabel* label = new QLabel(message);
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label, 1);

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->addStretch(1);
  if (reconnect && !quit) {
    QPushButton* reconnectBtn = new QPushButton(_("Reconnect"));
    btnsLayout->addWidget(reconnectBtn, 0, Qt::AlignRight);
    connect(reconnectBtn, &QPushButton::clicked, this, [=]() {
      accept();
      AppManager::instance()->connectToServer();
    });
  }
  QPushButton* closeBtn = new QPushButton(_("Close"));
  btnsLayout->addWidget(closeBtn, 0, Qt::AlignRight);
  connect(closeBtn, &QPushButton::clicked, this, [=]() {
    if (quit) {
      qApp->quit();
    }
    reject();
  });
  layout->addLayout(btnsLayout);

  setLayout(layout);
}
