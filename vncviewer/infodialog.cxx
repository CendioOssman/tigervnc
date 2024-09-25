#include "infodialog.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "appmanager.h"
#include "vncconnection.h"
#include "i18n.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

InfoDialog::InfoDialog(QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(_("VNC connection info"));

  QTimer* timer = new QTimer(this);
  timer->setSingleShot(false);
  timer->start(500);

  QVBoxLayout* layout = new QVBoxLayout;
  QLabel* label = new QLabel;
  label->setText(AppManager::instance()->getConnection()->infoText());
  layout->addWidget(label, 1);
  QPushButton* closeBtn = new QPushButton(_("Close"));
  layout->addWidget(closeBtn, 0, Qt::AlignRight);
  setLayout(layout);

  connect(timer, &QTimer::timeout, this, [=]() {
    label->setText(AppManager::instance()->getConnection()->infoText());
  });
  connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}
