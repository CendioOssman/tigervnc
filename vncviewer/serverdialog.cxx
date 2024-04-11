#include "serverdialog.h"

#include "appmanager.h"
#include "viewerconfig.h"
#include "i18n.h"

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QVBoxLayout>

ServerDialog::ServerDialog(QWidget* parent)
  : QWidget{parent}
{
  setWindowTitle(_("VNC Viewer: Connection Details"));

  QVBoxLayout* layout = new QVBoxLayout;

  QHBoxLayout* row1 = new QHBoxLayout;
  row1->addWidget(new QLabel(_("VNC server:")));
  comboBox = new QComboBox;
  comboBox->setEditable(true);
  comboBox->setFocus();
  row1->addWidget(comboBox, 1);
  layout->addLayout(row1);

  QHBoxLayout* row2 = new QHBoxLayout;
  QPushButton* optionsBtn = new QPushButton(_("Options..."));
  row2->addWidget(optionsBtn);
  QPushButton* loadBtn = new QPushButton(_("Load..."));
  row2->addWidget(loadBtn);
  QPushButton* saveAsBtn = new QPushButton(_("Save As..."));
  row2->addWidget(saveAsBtn);
  layout->addLayout(row2);

  QHBoxLayout* row3 = new QHBoxLayout;
  QPushButton* aboutBtn = new QPushButton(_("About..."));
  row3->addWidget(aboutBtn);
  QPushButton* cancelBtn = new QPushButton(_("Cancel"));
  row3->addWidget(cancelBtn);
  QPushButton* connectBtn = new QPushButton(_("Connect"));
  row3->addWidget(connectBtn);
  layout->addLayout(row3);

  setLayout(layout);

  connect(comboBox->lineEdit(), &QLineEdit::returnPressed, this, &ServerDialog::connectTo);

  connect(optionsBtn, &QPushButton::clicked, this, &ServerDialog::openOptionDialog);
  connect(loadBtn, &QPushButton::clicked, this, &ServerDialog::openLoadConfigDialog);
  connect(saveAsBtn, &QPushButton::clicked, this, &ServerDialog::openSaveConfigDialog);

  connect(aboutBtn, &QPushButton::clicked, this, &ServerDialog::openAboutDialog);
  connect(cancelBtn, &QPushButton::clicked, qApp, &QApplication::quit);
  connect(connectBtn, &QPushButton::clicked, this, &ServerDialog::connectTo);
  
  updateServerList(ViewerConfig::instance()->getServerHistory());
}

void ServerDialog::updateServerList(QStringList list)
{
  QStringListModel* model = new QStringListModel();
  model->setStringList(list);
  comboBox->setModel(model);
}

void ServerDialog::validateServerText(QString text)
{
  auto model = qobject_cast<QStringListModel*>(comboBox->model());
  if (model && model->stringList().contains(text)) {
    comboBox->setCurrentText(text);
  } else {
    ViewerConfig::instance()->addServer(text);
  }
}

void ServerDialog::connectTo()
{
  QString text = comboBox->currentText();
  validateServerText(text);
  AppManager::instance()->connectToServer(text);
}

void ServerDialog::openOptionDialog()
{
  AppManager::instance()->openOptionDialog();
}

void ServerDialog::openAboutDialog()
{
  AppManager::instance()->openAboutDialog();
}

void ServerDialog::openLoadConfigDialog()
{
  QString filename = QFileDialog::getOpenFileName(this,
                                                  _("Select a TigerVNC configuration file"),
                                                  {},
                                                  _("TigerVNC configuration (*.tigervnc);;All files (*)"));
  if (!filename.isEmpty()) {
    QString server = ViewerConfig::instance()->loadViewerParameters(filename);
    validateServerText(server);
  }
}

void ServerDialog::openSaveConfigDialog()
{
  QString filename = QFileDialog::getSaveFileName(this,
                                                  _("Save the TigerVNC configuration file"),
                                                  {},
                                                  _("TigerVNC configuration (*.tigervnc);;All files (*)"));
  if (!filename.isEmpty()) {
    ViewerConfig::instance()->saveViewerParameters(filename, comboBox->currentText());
  }
}
