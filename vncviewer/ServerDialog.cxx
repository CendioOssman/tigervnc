#include "ServerDialog.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "appmanager.h"
#include "viewerconfig.h"
#include "OptionsDialog.h"
#include "i18n.h"
#undef asprintf
#include "rfb/Exception.h"
#include "rfb/LogWriter.h"

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>

static rfb::LogWriter vlog("ServerDialog");

ServerDialog::ServerDialog(QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(_("VNC Viewer: Connection Details"));
#ifdef __APPLE__
  setWindowFlag(Qt::CustomizeWindowHint, true);
  setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  setWindowFlag(Qt::WindowFullscreenButtonHint, false);
#endif

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
  connect(cancelBtn, &QPushButton::clicked, this, &ServerDialog::reject);
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
  ViewerConfig::instance()->setServer(text);
  try {
    ViewerConfig::instance()->saveViewerParameters("", text);
  } catch (rfb::Exception& e) {
    vlog.error("%s", e.str());
    AppManager::instance()->publishError(QString::asprintf(_("Unable to save the default configuration:\n\n%s"),
                                                            e.str()));
  }
  accept();
}

void ServerDialog::openOptionDialog()
{
  OptionsDialog* d = new OptionsDialog(isFullScreen(), this);
  AppManager::instance()->openDialog(d);
}

void ServerDialog::openAboutDialog()
{
  ViewerConfig::aboutDialog(this);
}

void ServerDialog::openLoadConfigDialog()
{
  QString filename = QFileDialog::getOpenFileName(this,
                                                  _("Select a TigerVNC configuration file"),
                                                  {},
                                                  _("TigerVNC configuration (*.tigervnc);;All files (*)"));
  if (!filename.isEmpty()) {
    try {
      QString server = ViewerConfig::instance()->loadViewerParameters(filename);
      validateServerText(server);
    } catch (rfb::Exception& e) {
      QMessageBox* dlg;

      vlog.error("%s", e.str());

      dlg = new QMessageBox(QMessageBox::Critical,
                            _("Unable to load configuration"),
                            QString::asprintf(_("Unable to load the specified configuration file:\n\n%s"), e.str()),
                            QMessageBox::Ok, this);
      AppManager::instance()->openDialog(dlg);
    }
  }
}

void ServerDialog::openSaveConfigDialog()
{
  QString filename = QFileDialog::getSaveFileName(this,
                                                  _("Save the TigerVNC configuration to file"),
                                                  {},
                                                  _("TigerVNC configuration (*.tigervnc);;All files (*)"),
                                                  nullptr,
                                                  QFileDialog::DontConfirmOverwrite);
  if (!filename.isEmpty()) {
    QFile f(filename);

    if (f.open(QIODevice::ReadOnly)) {
      // The file already exists.
      f.close();

      QMessageBox* question = new QMessageBox(this);
      question->setWindowTitle("");
      question->addButton(_("Overwrite"), QMessageBox::AcceptRole);
      question->addButton(_("No"), QMessageBox::RejectRole);
      question->setText(QString::asprintf(_("%s already exists. Do you want to overwrite?"), filename.toStdString().c_str()));
      if (question->exec() == QMessageBox::RejectRole) {
        // If the user doesn't want to overwrite:
        openSaveConfigDialog();
        return;
      }
    }

    try {
      ViewerConfig::instance()->saveViewerParameters(filename, comboBox->currentText());
    } catch (rfb::Exception& e) {
      QMessageBox* dlg;

      vlog.error("%s", e.str());

      dlg = new QMessageBox(QMessageBox::Critical,
                            _("Unable to save configuration"),
                            QString::asprintf(_("Unable to save the specified configuration file:\n\n%s"), e.str()),
                            QMessageBox::Ok, this);
      AppManager::instance()->openDialog(dlg);
    }
  }
}
