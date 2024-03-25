#include "securitytab.h"
#include "parameters.h"

#include <QVBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>

SecurityTab::SecurityTab(QWidget *parent)
    : TabElement{parent}
{
    QVBoxLayout* layout = new QVBoxLayout;

    QGroupBox *groupBox1 = new QGroupBox(tr("Encryption"));
    QVBoxLayout *vbox1 = new QVBoxLayout;
    securityEncryptionNone = new QCheckBox(tr("None"));
    vbox1->addWidget(securityEncryptionNone);
    securityEncryptionTLSWithAnonymousCerts = new QCheckBox(tr("TLS with anonymous certificates"));
    securityEncryptionTLSWithAnonymousCerts->setEnabled(ViewerConfig::config()->haveGNUTLS());
    vbox1->addWidget(securityEncryptionTLSWithAnonymousCerts);
    securityEncryptionTLSWithX509Certs = new QCheckBox(tr("TLS with X509 certificates"));
    securityEncryptionTLSWithX509Certs->setEnabled(ViewerConfig::config()->haveGNUTLS());
    vbox1->addWidget(securityEncryptionTLSWithX509Certs);
    QLabel* securityEncryptionTLSWithX509CALabel = new QLabel(tr("Path to X509 CA certificate"));
    vbox1->addWidget(securityEncryptionTLSWithX509CALabel);
    securityEncryptionTLSWithX509CATextEdit = new QLineEdit;
    securityEncryptionTLSWithX509CATextEdit->setEnabled(ViewerConfig::config()->haveGNUTLS());
    vbox1->addWidget(securityEncryptionTLSWithX509CATextEdit);
    QLabel* securityEncryptionTLSWithX509CRLLabel = new QLabel(tr("Path to X509 CRL file"));
    vbox1->addWidget(securityEncryptionTLSWithX509CRLLabel);
    securityEncryptionTLSWithX509CRLTextEdit = new QLineEdit;
    securityEncryptionTLSWithX509CRLTextEdit->setEnabled(ViewerConfig::config()->haveGNUTLS());
    vbox1->addWidget(securityEncryptionTLSWithX509CRLTextEdit);
    securityEncryptionAES = new QCheckBox(tr("RSA-AES"));
    securityEncryptionAES->setEnabled(ViewerConfig::config()->haveNETTLE());
    vbox1->addWidget(securityEncryptionAES);
    groupBox1->setLayout(vbox1);
    layout->addWidget(groupBox1);

    QGroupBox *groupBox2 = new QGroupBox(tr("Authentication"));
    QVBoxLayout *vbox2 = new QVBoxLayout;
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

    setEnabled(ViewerConfig::config()->haveGNUTLS() || ViewerConfig::config()->haveNETTLE());

    connect(securityEncryptionTLSWithX509Certs, &QCheckBox::toggled, this, [=](bool checked){
        securityEncryptionTLSWithX509CATextEdit->setEnabled(checked);
        securityEncryptionTLSWithX509CRLTextEdit->setEnabled(checked);
    });
    connect(securityEncryptionAES, &QCheckBox::toggled, this, [=](bool checked){
        if(checked)
        {
            securityAuthenticationStandard->setChecked(checked);
            securityAuthenticationUsernameAndPassword->setChecked(checked);
        }
    });
}

void SecurityTab::apply()
{
    ViewerConfig::config()->setEncNone(securityEncryptionNone->isChecked());
    ViewerConfig::config()->setEncTLSAnon(securityEncryptionTLSWithAnonymousCerts->isChecked());
    ViewerConfig::config()->setEncTLSX509(securityEncryptionTLSWithX509Certs->isChecked());
    ViewerConfig::config()->setX509CA(securityEncryptionTLSWithX509CATextEdit->text());
    ViewerConfig::config()->setX509CRL(securityEncryptionTLSWithX509CRLTextEdit->text());
    ViewerConfig::config()->setEncAES(securityEncryptionAES->isChecked());
    ViewerConfig::config()->setAuthNone(securityAuthenticationNone->isChecked());
    ViewerConfig::config()->setAuthVNC(securityAuthenticationStandard->isChecked());
    ViewerConfig::config()->setAuthPlain(securityAuthenticationUsernameAndPassword->isChecked());
}

void SecurityTab::reset()
{
    securityEncryptionNone->setChecked(ViewerConfig::config()->encNone());
    securityEncryptionTLSWithAnonymousCerts->setChecked(ViewerConfig::config()->encTLSAnon());
    securityEncryptionTLSWithX509Certs->setChecked(ViewerConfig::config()->encTLSX509());
    securityEncryptionTLSWithX509CATextEdit->setText(ViewerConfig::config()->x509CA());
    securityEncryptionTLSWithX509CRLTextEdit->setText(ViewerConfig::config()->x509CRL());
    securityEncryptionAES->setChecked(ViewerConfig::config()->encAES());
    securityAuthenticationNone->setChecked(ViewerConfig::config()->authNone());
    securityAuthenticationStandard->setChecked(ViewerConfig::config()->authVNC());
    securityAuthenticationUsernameAndPassword->setChecked(ViewerConfig::config()->authPlain());
}
