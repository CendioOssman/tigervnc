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

#include <QBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

#include <rfb/Configuration.h>

#include "AuthDialog.h"
#include "parameters.h"
#include "i18n.h"

AuthDialog::AuthDialog(bool secure_, bool needsUser, bool needsPassword,
                       QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(_("VNC authentication"));

  QBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);

  QLabel* banner = new QLabel;
  banner->setFixedHeight(20);
  banner->setAlignment(Qt::AlignCenter);
  if (secure_) {
    std::string msg;
    msg = "<img src=':/secure.svg' style='vertical-align: middle;' />";
    msg += " ";
    msg += _("This connection is secure");
    banner->setText(msg.c_str());
    banner->setStyleSheet("QLabel { background-color: green; "
                          "color: black; font-size: 14px; "
                          "padding: 0 12px; }");
  } else {
    std::string msg;
    msg = "<img src=':/insecure.svg' style='vertical-align: middle;' />";
    msg += " ";
    msg += _("This connection is not secure");
    banner->setText(msg.c_str());
    banner->setStyleSheet("QLabel { background-color: red; "
                          "color: black; font-size: 14px; "
                          "padding: 0 12px; }");
  }
  layout->addWidget(banner);

  // FIXME: Add icon to match QMessageBox

  QFormLayout* formLayout = new QFormLayout;
  formLayout->setContentsMargins(5, 5, 5, 5);
  layout->addLayout(formLayout);

  if (needsUser) {
    username = new QLineEdit;
    formLayout->addRow(_("Username:"), username);
  } else {
    username = nullptr;
  }

  if (needsPassword) {
    passwd = new QLineEdit;
    passwd->setEchoMode(QLineEdit::Password);
    formLayout->addRow(_("Password:"), passwd);

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

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->setContentsMargins(5, 5, 5, 5);

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
