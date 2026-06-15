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

#include "OptionsSecurity.h"

#include "rfb/Security.h"
#include "rfb/SecurityClient.h"
#ifdef HAVE_GNUTLS
#include "rfb/CSecurityTLS.h"
#endif
#include "i18n.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

OptionsSecurity::OptionsSecurity(QWidget* parent)
  : OptionsPage{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  QGroupBox* groupBox1 = new QGroupBox(_("Encryption"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  encNoneCheckbox = new QCheckBox(_("None"));
  vbox1->addWidget(encNoneCheckbox);
  encTLSCheckbox = new QCheckBox(_("TLS with anonymous certificates"));
#ifndef HAVE_GNUTLS
  encTLSCheckbox->setVisible(false);
#endif
  vbox1->addWidget(encTLSCheckbox);
  encX509Checkbox = new QCheckBox(_("TLS with X509 certificates"));
#ifndef HAVE_GNUTLS
  encX509Checkbox->setVisible(false);
#endif
  vbox1->addWidget(encX509Checkbox);
  QHBoxLayout* h2 = new QHBoxLayout;
  h2->addSpacing(20);
  QLabel* securityEncryptionTLSWithX509CALabel = new QLabel(_("Path to X509 CA certificate"));
  h2->addWidget(securityEncryptionTLSWithX509CALabel);
  vbox1->addLayout(h2);
  QHBoxLayout* h3 = new QHBoxLayout;
  h3->addSpacing(20);
  caInput = new QLineEdit;
#ifndef HAVE_GNUTLS
  securityEncryptionTLSWithX509CALabel->setVisible(false);
  caInput->setVisible(false);
#endif
  h3->addWidget(caInput);
  vbox1->addLayout(h3);
  QHBoxLayout* h4 = new QHBoxLayout;
  h4->addSpacing(20);
  QLabel* securityEncryptionTLSWithX509CRLLabel = new QLabel(_("Path to X509 CRL file"));
  h4->addWidget(securityEncryptionTLSWithX509CRLLabel);
  vbox1->addLayout(h4);
  QHBoxLayout* h5 = new QHBoxLayout;
  h5->addSpacing(20);
  crlInput = new QLineEdit;
#ifndef HAVE_GNUTLS
  securityEncryptionTLSWithX509CRLLabel->setVisible(false);
  crlInput->setVisible(false);
#endif
  h5->addWidget(crlInput);
  vbox1->addLayout(h5);
  encRSAAESCheckbox = new QCheckBox(_("RSA-AES"));
#ifndef HAVE_NETTLE
  encRSAAESCheckbox->setVisible(false);
#endif
  vbox1->addWidget(encRSAAESCheckbox);
  groupBox1->setLayout(vbox1);
  layout->addWidget(groupBox1);

  QGroupBox* groupBox2 = new QGroupBox(_("Authentication"));
  QVBoxLayout* vbox2 = new QVBoxLayout;
  authNoneCheckbox = new QCheckBox(_("None"));
  vbox2->addWidget(authNoneCheckbox);
  authVncCheckbox = new QCheckBox(_("Standard VNC (insecure without encryption)"));
  vbox2->addWidget(authVncCheckbox);
  authPlainCheckbox = new QCheckBox(_("Username and password (insecure without encryption)"));
  vbox2->addWidget(authPlainCheckbox);
  groupBox2->setLayout(vbox2);
  layout->addWidget(groupBox2);

  layout->addStretch(1);
  setLayout(layout);

  connect(encX509Checkbox, &QCheckBox::toggled, this, [=](bool checked) {
    caInput->setEnabled(checked);
    crlInput->setEnabled(checked);
  });
  connect(encRSAAESCheckbox, &QCheckBox::toggled, this, [=](bool checked) {
    if (checked) {
      authVncCheckbox->setChecked(checked);
      authPlainCheckbox->setChecked(checked);
    }
  });
}

void OptionsSecurity::apply()
{
  /* Security */
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

  rfb::CSecurityTLS::X509CA.setParam(caInput->text().toStdString().c_str());
  rfb::CSecurityTLS::X509CRL.setParam(crlInput->text().toStdString().c_str());
#endif

#ifdef HAVE_NETTLE
  if (encRSAAESCheckbox->isChecked()) {
    security.EnableSecType(rfb::secTypeRA2);
    security.EnableSecType(rfb::secTypeRA256);
  }
#endif
  rfb::SecurityClient::secTypes.setParam(security.ToString());
}

void OptionsSecurity::reset()
{
  rfb::Security security(rfb::SecurityClient::secTypes);
  std::list<uint8_t> secTypes = security.GetEnabledSecTypes();
  for (uint8_t secType : secTypes) {
    switch (secType) {
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

  std::list<uint32_t> secTypesExt = security.GetEnabledExtSecTypes();
  for (uint32_t secTypeExt : secTypesExt) {
    switch (secTypeExt) {
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
  caInput->setText(rfb::CSecurityTLS::X509CA.getValueStr().c_str());
  crlInput->setText(rfb::CSecurityTLS::X509CRL.getValueStr().c_str());
#endif
}
