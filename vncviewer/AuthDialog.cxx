/* Copyright 2024 Adam Halim for Cendio AB
 * Copyright 2011-2026 Pierre Ossman for Cendio AB
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

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <qboxlayout.h>
#include <qlayout.h>

#include "AuthDialog.h"
#include "parameters.h"
#include "i18n.h"

AuthDialog::AuthDialog(bool secure_, bool needsUser, bool needsPassword,
                       QWidget* parent)
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
  layout->setContentsMargins(0, 0, 0, 0);

  QLabel* banner = new QLabel;
  banner->setAlignment(Qt::AlignCenter);
  if (secure_) {
    banner->setText(QString("<img src=':/secure.svg' style='vertical-align: middle;' />") + _("This connection is secure"));
    banner->setStyleSheet("QLabel { background-color: '#ff00ff00'; color: 'black'; font-size: 14px; }");
  } else {
    banner->setText(QString("<img src=':/insecure.svg' style='vertical-align: middle;' />") + _("This connection is not secure"));
    banner->setStyleSheet("QLabel { background-color: '#ffff0000'; color: 'black'; font-size: 14px; }");
  }
  layout->addWidget(banner, 1);

  QFormLayout* formLayout = new QFormLayout;
  formLayout->setContentsMargins(5, 5, 5, 5);

  if (needsUser) {
    username = new QLineEdit;
    username->setFocus();
    formLayout->addRow(_("Username:"), username);
  } else {
    username = nullptr;
  }

  if (needsPassword) {
    passwd = new QLineEdit;
    passwd->setEchoMode(QLineEdit::Password);
    passwd->setFocus();
    formLayout->addRow(_("Password:"), passwd);
    connect(passwd, &QLineEdit::returnPressed, this, &AuthDialog::accept);

    if (reconnectOnError) {
      keepPasswdCheckbox =
        new QCheckBox(_("Keep password for reconnect"));
      formLayout->addRow(keepPasswdCheckbox);
    } else {
      keepPasswdCheckbox = nullptr;
    }
  } else {
    passwd = nullptr;
    keepPasswdCheckbox = nullptr;
  }

  layout->addLayout(formLayout);

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->setContentsMargins(5, 5, 5, 5);
  btnsLayout->addStretch(1);

  QPushButton* cancelBtn = new QPushButton(_("Cancel"));
  connect(cancelBtn, &QPushButton::clicked, this, &AuthDialog::reject);
  btnsLayout->addWidget(cancelBtn, 0, Qt::AlignRight);

  QPushButton* okBtn = new QPushButton(_("Ok"));
  connect(okBtn, &QPushButton::clicked, this, &AuthDialog::accept);
  btnsLayout->addWidget(okBtn, 0, Qt::AlignRight);

  layout->addLayout(btnsLayout);

  setLayout(layout);
}

std::string AuthDialog::getUser()
{
  if (username)
    return username->text().toStdString();
  return "";
}

std::string AuthDialog::getPassword()
{
  if (passwd)
    return passwd->text().toStdString();
  return "";
}

bool AuthDialog::getKeepPassword()
{
  if (keepPasswdCheckbox)
    return keepPasswdCheckbox->isChecked();
  return false;
}
