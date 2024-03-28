#include "serverdialog.h"

#include "appmanager.h"
#include "parameters.h"

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
  setWindowTitle(tr("VNC Viewer: Connection Details"));

  QVBoxLayout* layout = new QVBoxLayout;

  QHBoxLayout* row1 = new QHBoxLayout;
  row1->addWidget(new QLabel(tr("VNC server:")));
  comboBox = new QComboBox;
  comboBox->setEditable(true);
  comboBox->setFocus();
  row1->addWidget(comboBox, 1);
  layout->addLayout(row1);

  QHBoxLayout* row2 = new QHBoxLayout;
  QPushButton* optionsBtn = new QPushButton(tr("Options..."));
  row2->addWidget(optionsBtn);
  QPushButton* loadBtn = new QPushButton(tr("Load..."));
  row2->addWidget(loadBtn);
  QPushButton* saveAsBtn = new QPushButton(tr("Save As..."));
  row2->addWidget(saveAsBtn);
  layout->addLayout(row2);

  QHBoxLayout* row3 = new QHBoxLayout;
  QPushButton* aboutBtn = new QPushButton(tr("About..."));
  row3->addWidget(aboutBtn);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
  row3->addWidget(cancelBtn);
  QPushButton* connectBtn = new QPushButton(tr("Connect"));
  row3->addWidget(connectBtn);
  layout->addLayout(row3);

  setLayout(layout);

  connect(ViewerConfig::config(), &ViewerConfig::serverHistoryChanged, this, &ServerDialog::updateServerList);
  connect(comboBox->lineEdit(), &QLineEdit::returnPressed, this, &ServerDialog::connectTo);

  connect(optionsBtn, &QPushButton::clicked, this, &ServerDialog::openOptionDialog);
  connect(loadBtn, &QPushButton::clicked, this, &ServerDialog::openLoadConfigDialog);
  connect(saveAsBtn, &QPushButton::clicked, this, &ServerDialog::openSaveConfigDialog);

  connect(aboutBtn, &QPushButton::clicked, this, &ServerDialog::openAboutDialog);
  connect(cancelBtn, &QPushButton::clicked, qApp, &QApplication::quit);
  connect(connectBtn, &QPushButton::clicked, this, &ServerDialog::connectTo);

  updateServerList(ViewerConfig::config()->serverHistory());
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
    ViewerConfig::config()->addToServerHistory(text);
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
                                                  tr("Load Config"),
                                                  {},
                                                  tr("TigerVNC configuration (*.tigervnc);;All files (*)"));
  if (!filename.isEmpty()) {
    QString server = ViewerConfig::config()->loadViewerParameters(filename);
    validateServerText(server);
  }
}

void ServerDialog::openSaveConfigDialog()
{
  QString filename = QFileDialog::getSaveFileName(this,
                                                  tr("Save Config"),
                                                  {},
                                                  tr("TigerVNC configuration (*.tigervnc);;All files (*)"));
  if (!filename.isEmpty()) {
    ViewerConfig::config()->saveViewerParameters(filename, comboBox->currentText());
  }
}
