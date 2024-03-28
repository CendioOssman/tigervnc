#include "messagedialog.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

MessageDialog::MessageDialog(int flags, QString title, QString text, QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(title);

  QVBoxLayout* layout = new QVBoxLayout;
  QLabel* label = new QLabel;
  label->setText(text);
  label->setAlignment(Qt::AlignVCenter);
  layout->addWidget(label, 1);
  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->addStretch(1);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
  btnsLayout->addWidget(cancelBtn, 0, Qt::AlignRight);
  layout->addLayout(btnsLayout);
  QPushButton* okBtn = new QPushButton(tr("Ok"));
  btnsLayout->addWidget(okBtn, 0, Qt::AlignRight);
  layout->addLayout(btnsLayout);
  setLayout(layout);

  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}
