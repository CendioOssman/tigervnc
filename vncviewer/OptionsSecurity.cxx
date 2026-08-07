/* Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
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
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <rfb/Security.h>
#include <rfb/SecurityClient.h>
#ifdef HAVE_GNUTLS
#include <rfb/CSecurityTLS.h>
#endif

#include "OptionsSecurity.h"
#include "i18n.h"

OptionsSecurity::OptionsSecurity(QWidget* parent)
  : OptionsPage(parent)
{
  QBoxLayout* indent;
  QFormLayout* form;
  QBoxLayout* layout = new QVBoxLayout;

  /* Encryption */
  encryptionGroup = new QGroupBox(_("Encryption"));
  layout->addWidget(encryptionGroup);

  {
    QBoxLayout* box = new QVBoxLayout;
    encryptionGroup->setLayout(box);

    encNoneCheckbox = new QCheckBox(_("None"));
    box->addWidget(encNoneCheckbox);

#ifdef HAVE_GNUTLS
    encTLSCheckbox = new QCheckBox(_("TLS with anonymous certificates"));
    box->addWidget(encTLSCheckbox);

    encX509Checkbox = new QCheckBox(_("TLS with X509 certificates"));
    connect(encX509Checkbox, &QCheckBox::toggled,
            this, &OptionsSecurity::handleX509);
    box->addWidget(encX509Checkbox);

    indent = new QHBoxLayout;
    indent->addSpacing(20);
    form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    indent->addLayout(form);
    caInput = new QLineEdit;
    form->addRow(_("Path to X509 CA certificate"), caInput);
    box->addLayout(indent);

    indent = new QHBoxLayout;
    indent->addSpacing(20);
    form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    indent->addLayout(form);
    crlInput = new QLineEdit;
    form->addRow(_("Path to X509 CRL file"), crlInput);
    box->addLayout(indent);
#endif

#ifdef HAVE_NETTLE
    encRSAAESCheckbox = new QCheckBox(_("RSA-AES"));
    connect(encRSAAESCheckbox, &QCheckBox::toggled,
            this, &OptionsSecurity::handleRSAAES);
    box->addWidget(encRSAAESCheckbox);
#endif
  }

  /* Authentication */
  authenticationGroup = new QGroupBox(_("Authentication"));
  layout->addWidget(authenticationGroup);

  {
    QBoxLayout* box = new QVBoxLayout;
    authenticationGroup->setLayout(box);

    authNoneCheckbox = new QCheckBox(_("None"));
    box->addWidget(authNoneCheckbox);

    authVncCheckbox = new QCheckBox(_("Standard VNC (insecure without encryption)"));
    box->addWidget(authVncCheckbox);

    authPlainCheckbox = new QCheckBox(_("Username and password (insecure without encryption)"));
    box->addWidget(authPlainCheckbox);
  }

  layout->addStretch(1);

  setLayout(layout);
}

void OptionsSecurity::loadOptions()
{
  rfb::Security security(rfb::SecurityClient::secTypes);

  std::list<uint8_t> secTypes;

  std::list<uint32_t> secTypesExt;

  encNoneCheckbox->setChecked(false);
#ifdef HAVE_GNUTLS
  encTLSCheckbox->setChecked(false);
  encX509Checkbox->setChecked(false);
#endif
#ifdef HAVE_NETTLE
  encRSAAESCheckbox->setChecked(false);
#endif

  authNoneCheckbox->setChecked(false);
  authVncCheckbox->setChecked(false);
  authPlainCheckbox->setChecked(false);

  secTypes = security.GetEnabledSecTypes();
  for (uint8_t type : secTypes) {
    switch (type) {
    case rfb::secTypeNone:
      encNoneCheckbox->setChecked(true);
      authNoneCheckbox->setChecked(true);
      break;
    case rfb::secTypeVncAuth:
      encNoneCheckbox->setChecked(true);
      authVncCheckbox->setChecked(true);
      break;
    }
  }

  secTypesExt = security.GetEnabledExtSecTypes();
  for (uint32_t type : secTypesExt) {
    switch (type) {
    case rfb::secTypePlain:
      encNoneCheckbox->setChecked(true);
      authPlainCheckbox->setChecked(true);
      break;
#ifdef HAVE_GNUTLS
    case rfb::secTypeTLSNone:
      encTLSCheckbox->setChecked(true);
      authNoneCheckbox->setChecked(true);
      break;
    case rfb::secTypeTLSVnc:
      encTLSCheckbox->setChecked(true);
      authVncCheckbox->setChecked(true);
      break;
    case rfb::secTypeTLSPlain:
      encTLSCheckbox->setChecked(true);
      authPlainCheckbox->setChecked(true);
      break;
    case rfb::secTypeX509None:
      encX509Checkbox->setChecked(true);
      authNoneCheckbox->setChecked(true);
      break;
    case rfb::secTypeX509Vnc:
      encX509Checkbox->setChecked(true);
      authVncCheckbox->setChecked(true);
      break;
    case rfb::secTypeX509Plain:
      encX509Checkbox->setChecked(true);
      authPlainCheckbox->setChecked(true);
      break;
#endif
#ifdef HAVE_NETTLE
    case rfb::secTypeRA2:
    case rfb::secTypeRA256:
      encRSAAESCheckbox->setChecked(true);
      authVncCheckbox->setChecked(true);
      authPlainCheckbox->setChecked(true);
      break;
    case rfb::secTypeRA2ne:
    case rfb::secTypeRAne256:
      authVncCheckbox->setChecked(true);
      /* fall through */
    case rfb::secTypeDH:
    case rfb::secTypeMSLogonII:
      encNoneCheckbox->setChecked(true);
      authPlainCheckbox->setChecked(true);
      break;
#endif

    }
  }

#ifdef HAVE_GNUTLS
  caInput->setText((const char*)rfb::CSecurityTLS::X509CA);
  crlInput->setText((const char*)rfb::CSecurityTLS::X509CRL);

  handleX509();
#endif
}

void OptionsSecurity::storeOptions()
{
  rfb::Security security;

  /* Process security types which don't use encryption */
  if (encNoneCheckbox->isChecked()) {
    if (authNoneCheckbox->isChecked())
      security.EnableSecType(rfb::secTypeNone);
    if (authVncCheckbox->isChecked()) {
      security.EnableSecType(rfb::secTypeVncAuth);
#ifdef HAVE_NETTLE
      security.EnableSecType(rfb::secTypeRA2ne);
      security.EnableSecType(rfb::secTypeRAne256);
#endif
    }
    if (authPlainCheckbox->isChecked()) {
      security.EnableSecType(rfb::secTypePlain);
#ifdef HAVE_NETTLE
      security.EnableSecType(rfb::secTypeRA2ne);
      security.EnableSecType(rfb::secTypeRAne256);
      security.EnableSecType(rfb::secTypeDH);
      security.EnableSecType(rfb::secTypeMSLogonII);
#endif
    }
  }

#ifdef HAVE_GNUTLS
  /* Process security types which use TLS encryption */
  if (encTLSCheckbox->isChecked()) {
    if (authNoneCheckbox->isChecked())
      security.EnableSecType(rfb::secTypeTLSNone);
    if (authVncCheckbox->isChecked())
      security.EnableSecType(rfb::secTypeTLSVnc);
    if (authPlainCheckbox->isChecked())
      security.EnableSecType(rfb::secTypeTLSPlain);
  }

  /* Process security types which use X509 encryption */
  if (encX509Checkbox->isChecked()) {
    if (authNoneCheckbox->isChecked())
      security.EnableSecType(rfb::secTypeX509None);
    if (authVncCheckbox->isChecked())
      security.EnableSecType(rfb::secTypeX509Vnc);
    if (authPlainCheckbox->isChecked())
      security.EnableSecType(rfb::secTypeX509Plain);
  }

  rfb::CSecurityTLS::X509CA.setParam(caInput->text().toStdString());
  rfb::CSecurityTLS::X509CRL.setParam(crlInput->text().toStdString());
#endif

#ifdef HAVE_NETTLE
  if (encRSAAESCheckbox->isChecked()) {
    security.EnableSecType(rfb::secTypeRA2);
    security.EnableSecType(rfb::secTypeRA256);
  }
#endif
  rfb::SecurityClient::secTypes.setParam(security.ToString());
}

void OptionsSecurity::handleX509()
{
  caInput->setEnabled(encX509Checkbox->isChecked());
  crlInput->setEnabled(encX509Checkbox->isChecked());
}

void OptionsSecurity::handleRSAAES()
{
  if (encRSAAESCheckbox->isChecked()) {
    authVncCheckbox->setChecked(true);
    authPlainCheckbox->setChecked(true);
  }
}
