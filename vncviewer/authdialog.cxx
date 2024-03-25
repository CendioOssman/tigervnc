#include "authdialog.h"
#include "appmanager.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QLineEdit>

AuthDialog::AuthDialog(bool secured, bool userNeeded, bool passwordNeeded, QWidget *parent)
    : QDialog{parent}
{
    setWindowTitle(tr("VNC authentication"));

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setMargin(0);

    QLabel* securedLabel = new QLabel;
    securedLabel->setAlignment(Qt::AlignCenter);
    if (secured) {
        securedLabel->setText(tr("This connection is secure"));
        securedLabel->setStyleSheet("QLabel { background-color: '#ff00ff00'; color: 'black'; }");
    }
    else {
        securedLabel->setText(tr("This connection is not secure"));
        securedLabel->setStyleSheet("QLabel { background-color: '#ffff0000'; color: 'black'; }");
    }
    layout->addWidget(securedLabel, 1);

    QFormLayout* formLayout = new QFormLayout;
    if (userNeeded) {
        userText = new QLineEdit;
        userText->setFocus();
        formLayout->addRow(tr("Username"), userText);
    }
    else if (passwordNeeded) {
        passwordText = new QLineEdit;
        passwordText->setEchoMode(QLineEdit::Password);
        passwordText->setFocus();
        formLayout->addRow(tr("Password"), passwordText);
        connect(passwordText, &QLineEdit::returnPressed, this, &AuthDialog::ok);
    }
    layout->addLayout(formLayout);

    QHBoxLayout* btnsLayout = new QHBoxLayout;
    btnsLayout->addStretch(1);
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
    btnsLayout->addWidget(cancelBtn, 0, Qt::AlignRight);
    QPushButton* okBtn = new QPushButton(tr("Ok"));
    btnsLayout->addWidget(okBtn, 0, Qt::AlignRight);
    layout->addLayout(btnsLayout);

    setLayout(layout);

    connect(cancelBtn, &QPushButton::clicked, this, &AuthDialog::cancel);
    connect(okBtn, &QPushButton::clicked, this, &AuthDialog::ok);
}

void AuthDialog::ok()
{
    close();
    AppManager::instance()->authenticate(userText ? userText->text() : "", passwordText ? passwordText->text() : "");
}

void AuthDialog::cancel()
{
    close();
    AppManager::instance()->cancelAuth();
}
