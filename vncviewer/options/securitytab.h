#ifndef SECURITYTAB_H
#define SECURITYTAB_H

#include "../optionsdialog.h"

#include <QWidget>

class QCheckBox;
class QLineEdit;

class SecurityTab : public TabElement
{
  Q_OBJECT

public:
  SecurityTab(QWidget* parent = nullptr);

  void apply();
  void reset();

private:
  QCheckBox* securityEncryptionNone;
  QCheckBox* securityEncryptionTLSWithAnonymousCerts;
  QCheckBox* securityEncryptionTLSWithX509Certs;
  QLineEdit* securityEncryptionTLSWithX509CATextEdit;
  QLineEdit* securityEncryptionTLSWithX509CRLTextEdit;
  QCheckBox* securityAuthenticationNone;
  QCheckBox* securityAuthenticationStandard;
  QCheckBox* securityAuthenticationUsernameAndPassword;
  QCheckBox* securityEncryptionAES;
};

#endif // SECURITYTAB_H
