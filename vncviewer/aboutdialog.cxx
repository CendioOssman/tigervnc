#include "aboutdialog.h"

#include "viewerconfig.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(bool staysOnTop, QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(tr("About TigerVNC Viewer"));
  setWindowFlag(Qt::WindowStaysOnTopHint, staysOnTop);

  QVBoxLayout* layout = new QVBoxLayout;
  layout->addWidget(new QLabel(ViewerConfig::aboutText()));
  QPushButton* closeBtn = new QPushButton(tr("Close"));
  layout->addWidget(closeBtn, 0, Qt::AlignRight);
  setLayout(layout);

  connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}
