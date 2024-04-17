#include "authdialog.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "appmanager.h"
#include "i18n.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

AuthDialog::AuthDialog(bool secured, bool userNeeded, bool passwordNeeded, QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(_("VNC authentication"));
  setFixedSize(250, 100);
#ifdef __APPLE__
  setWindowFlag(Qt::CustomizeWindowHint, true);
  setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  setWindowFlag(Qt::WindowFullscreenButtonHint, false);
#endif

  QVBoxLayout* layout = new QVBoxLayout;
  layout->setMargin(0);

  QLabel* securedLabel = new QLabel;
  securedLabel->setAlignment(Qt::AlignCenter);
  if (secured) {
    securedLabel->setText(QString("<img src=':/secure.svg' style='vertical-align: middle;' />") + _("This connection is secure"));
    securedLabel->setStyleSheet("QLabel { background-color: '#ff00ff00'; color: 'black'; font-size: 14px; }");
  } else {
    securedLabel->setText(QString("<img src=':/insecure.svg' style='vertical-align: middle;' />") + _("This connection is not secure"));
    securedLabel->setStyleSheet("QLabel { background-color: '#ffff0000'; color: 'black'; font-size: 14px; }");
  }
  layout->addWidget(securedLabel, 1);

  QFormLayout* formLayout = new QFormLayout;
  formLayout->setMargin(5);
  if (userNeeded) {
    userText = new QLineEdit;
    userText->setFocus();
    formLayout->addRow(_("Username:"), userText);
  } else if (passwordNeeded) {
    passwordText = new QLineEdit;
    passwordText->setEchoMode(QLineEdit::Password);
    passwordText->setFocus();
    formLayout->addRow(_("Password:"), passwordText);
    connect(passwordText, &QLineEdit::returnPressed, this, &AuthDialog::accept);
  }
  layout->addLayout(formLayout);

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->setMargin(5);
  btnsLayout->addStretch(1);
  QPushButton* cancelBtn = new QPushButton(_("Cancel"));
  btnsLayout->addWidget(cancelBtn, 0, Qt::AlignRight);
  QPushButton* okBtn = new QPushButton(_("Ok"));
  btnsLayout->addWidget(okBtn, 0, Qt::AlignRight);
  layout->addLayout(btnsLayout);

  setLayout(layout);

  connect(cancelBtn, &QPushButton::clicked, this, &AuthDialog::reject);
  connect(okBtn, &QPushButton::clicked, this, &AuthDialog::accept);
}

void AuthDialog::accept()
{
  QDialog::accept();
  AppManager::instance()->authenticate(userText ? userText->text() : "", passwordText ? passwordText->text() : "");
}

void AuthDialog::reject()
{
  QDialog::reject();
  AppManager::instance()->cancelAuth();
}
