#include "aboutdialog.h"

#include "parameters.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(tr("About TigerVNC Viewer"));

  QVBoxLayout* layout = new QVBoxLayout;
  layout->addWidget(new QLabel(ViewerConfig::config()->aboutText()));
  QPushButton* closeBtn = new QPushButton(tr("Close"));
  layout->addWidget(closeBtn, 0, Qt::AlignRight);
  setLayout(layout);

  connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}
