#include "securitytab.h"

#include "viewerconfig.h"
#include "rfb/Security.h"
#include "rfb/SecurityClient.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

SecurityTab::SecurityTab(QWidget* parent)
  : TabElement{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  QGroupBox* groupBox1 = new QGroupBox(tr("Encryption"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  securityEncryptionNone = new QCheckBox(tr("None"));
  vbox1->addWidget(securityEncryptionNone);
  securityEncryptionTLSWithAnonymousCerts = new QCheckBox(tr("TLS with anonymous certificates"));
#ifndef HAVE_GNUTLS
  securityEncryptionTLSWithAnonymousCerts->setVisible(false);
#endif
  vbox1->addWidget(securityEncryptionTLSWithAnonymousCerts);
  securityEncryptionTLSWithX509Certs = new QCheckBox(tr("TLS with X509 certificates"));
#ifndef HAVE_GNUTLS
  securityEncryptionTLSWithX509Certs->setVisible(false);
#endif
  vbox1->addWidget(securityEncryptionTLSWithX509Certs);
  QLabel* securityEncryptionTLSWithX509CALabel = new QLabel(tr("Path to X509 CA certificate"));
  vbox1->addWidget(securityEncryptionTLSWithX509CALabel);
  securityEncryptionTLSWithX509CATextEdit = new QLineEdit;
#ifndef HAVE_GNUTLS
  securityEncryptionTLSWithX509CATextEdit->setVisible(false);
#endif
  vbox1->addWidget(securityEncryptionTLSWithX509CATextEdit);
  QLabel* securityEncryptionTLSWithX509CRLLabel = new QLabel(tr("Path to X509 CRL file"));
  vbox1->addWidget(securityEncryptionTLSWithX509CRLLabel);
  securityEncryptionTLSWithX509CRLTextEdit = new QLineEdit;
#ifndef HAVE_GNUTLS
  securityEncryptionTLSWithX509CRLTextEdit->setVisible(false);
#endif
  vbox1->addWidget(securityEncryptionTLSWithX509CRLTextEdit);
  securityEncryptionAES = new QCheckBox(tr("RSA-AES"));
#ifndef HAVE_NETTLE
  securityEncryptionAES->setVisible(false);
#endif
  vbox1->addWidget(securityEncryptionAES);
  groupBox1->setLayout(vbox1);
  layout->addWidget(groupBox1);

  QGroupBox* groupBox2 = new QGroupBox(tr("Authentication"));
  QVBoxLayout* vbox2 = new QVBoxLayout;
  securityAuthenticationNone = new QCheckBox(tr("None"));
  vbox2->addWidget(securityAuthenticationNone);
  securityAuthenticationStandard = new QCheckBox(tr("Standard VNC (insecure without encryption)"));
  vbox2->addWidget(securityAuthenticationStandard);
  securityAuthenticationUsernameAndPassword = new QCheckBox(tr("Username and password (insecure without encryption)"));
  vbox2->addWidget(securityAuthenticationUsernameAndPassword);
  groupBox2->setLayout(vbox2);
  layout->addWidget(groupBox2);

  layout->addStretch(1);
  setLayout(layout);

  connect(securityEncryptionTLSWithX509Certs, &QCheckBox::toggled, this, [=](bool checked) {
    securityEncryptionTLSWithX509CATextEdit->setEnabled(checked);
    securityEncryptionTLSWithX509CRLTextEdit->setEnabled(checked);
  });
  connect(securityEncryptionAES, &QCheckBox::toggled, this, [=](bool checked) {
    if (checked) {
      securityAuthenticationStandard->setChecked(checked);
      securityAuthenticationUsernameAndPassword->setChecked(checked);
    }
  });
}

void SecurityTab::apply()
{
  /* Security */
  rfb::Security security;

  /* Process security types which don't use encryption */
  if (securityEncryptionNone->isChecked()) {
    if (securityAuthenticationNone->isChecked())
      security.EnableSecType(rfb::secTypeNone);
    if (securityAuthenticationStandard->isChecked()) {
      security.EnableSecType(rfb::secTypeVncAuth);
#ifdef HAVE_NETTLE
      security.EnableSecType(secTypeRA2ne);
      security.EnableSecType(secTypeRAne256);
#endif
    }
    if (securityAuthenticationUsernameAndPassword->isChecked()) {
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
  if (securityEncryptionTLSWithAnonymousCerts->isChecked()) {
    if (securityAuthenticationNone->isChecked())
      security.EnableSecType(rfb::secTypeTLSNone);
    if (securityAuthenticationStandard->isChecked())
      security.EnableSecType(rfb::secTypeTLSVnc);
    if (securityAuthenticationUsernameAndPassword->isChecked())
      security.EnableSecType(rfb::secTypeTLSPlain);
  }

  /* Process security types which use X509 encryption */
  if (securityEncryptionTLSWithX509Certs->isChecked()) {
    if (securityAuthenticationNone->isChecked())
      security.EnableSecType(rfb::secTypeX509None);
    if (securityAuthenticationStandard->isChecked())
      security.EnableSecType(rfb::secTypeX509Vnc);
    if (securityAuthenticationUsernameAndPassword->isChecked())
      security.EnableSecType(rfb::secTypeX509Plain);
  }

  rfb::CSecurityTLS::X509CA.setParam(securityEncryptionTLSWithX509CATextEdit->text().toStdString().c_str());
  rfb::CSecurityTLS::X509CRL.setParam(securityEncryptionTLSWithX509CRLTextEdit->text().toStdString().c_str());
#endif

#ifdef HAVE_NETTLE
  if (securityEncryptionAES->isChecked()) {
    security.EnableSecType(rfb::secTypeRA2);
    security.EnableSecType(rfb::secTypeRA256);
  }
#endif
  rfb::SecurityClient::secTypes.setParam(security.ToString());
}

void SecurityTab::reset()
{
  rfb::Security security(rfb::SecurityClient::secTypes);
  auto secTypes = security.GetEnabledSecTypes();
  for (auto iter = secTypes.begin(); iter != secTypes.end(); ++iter) {
    switch (*iter) {
    case rfb::secTypeNone:
      securityEncryptionNone->setChecked(true);
      securityAuthenticationNone->setChecked(true);
      break;
    case rfb::secTypeVncAuth:
      securityEncryptionNone->setChecked(true);
      securityAuthenticationStandard->setChecked(true);
      break;
    }
  }

  auto secTypesExt = security.GetEnabledExtSecTypes();
  for (auto iterExt = secTypesExt.begin(); iterExt != secTypesExt.end(); ++iterExt) {
    switch (*iterExt) {
    case rfb::secTypePlain:
      securityEncryptionNone->setChecked(true);
      securityAuthenticationUsernameAndPassword->setChecked(true);
      break;
#ifdef HAVE_GNUTLS
    case rfb::secTypeTLSNone:
      securityEncryptionTLSWithAnonymousCerts->setChecked(true);
      securityAuthenticationNone->setChecked(true);
      break;
    case rfb::secTypeTLSVnc:
      securityEncryptionTLSWithAnonymousCerts->setChecked(true);
      securityAuthenticationStandard->setChecked(true);
      break;
    case rfb::secTypeTLSPlain:
      securityEncryptionTLSWithAnonymousCerts->setChecked(true);
      securityAuthenticationUsernameAndPassword->setChecked(true);
      break;
    case rfb::secTypeX509None:
      securityEncryptionTLSWithX509Certs->setChecked(true);
      securityAuthenticationNone->setChecked(true);
      break;
    case rfb::secTypeX509Vnc:
      securityEncryptionTLSWithX509Certs->setChecked(true);
      securityAuthenticationStandard->setChecked(true);
      break;
    case rfb::secTypeX509Plain:
      securityEncryptionTLSWithX509Certs->setChecked(true);
      securityAuthenticationUsernameAndPassword->setChecked(true);
      break;
#endif
#ifdef HAVE_NETTLE
    case rfb::secTypeRA2:
    case rfb::secTypeRA256:
      securityEncryptionAES->setChecked(true);
      securityAuthenticationStandard->setChecked(true);
      securityAuthenticationUsernameAndPassword->setChecked(true);
      break;
    case rfb::secTypeRA2ne:
    case rfb::secTypeRAne256:
      securityAuthenticationStandard->setChecked(true);
      /* fall through */
    case rfb::secTypeDH:
    case rfb::secTypeMSLogonII:
      securityEncryptionNone->setChecked(true);
      securityAuthenticationUsernameAndPassword->setChecked(true);
      break;
#endif
    }
  }

#ifdef HAVE_GNUTLS
  securityEncryptionTLSWithX509CATextEdit->setText(rfb::CSecurityTLS::X509CA);
  securityEncryptionTLSWithX509CRLTextEdit->setText(rfb::CSecurityTLS::X509CRL);
#endif
}
