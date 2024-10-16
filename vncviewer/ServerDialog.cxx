#include "ServerDialog.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "appmanager.h"
#include "viewerconfig.h"
#include "OptionsDialog.h"
#include "i18n.h"
#undef asprintf
#include "parameters.h"
#include "vncviewer.h"
#include "os/os.h"
#include "rfb/Exception.h"
#include "rfb/LogWriter.h"

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QTextStream>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>

static rfb::LogWriter vlog("ServerDialog");

const char* SERVER_HISTORY = "tigervnc.history";

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

  try {
    loadServerHistory();
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    QMessageBox* dlg = new QMessageBox(QMessageBox::Critical,
                                       _("Error loading server history"),
                                       QString::asprintf(_("Unable to load the server history:\n\n%s"), e.str()),
                                       QMessageBox::Close);
    AppManager::instance()->openDialog(dlg);
  }

  QStringListModel* model = new QStringListModel();
  model->setStringList(serverHistory);
  comboBox->setModel(model);
}

QString ServerDialog::getServerName()
{
  return comboBox->currentText();
}

void ServerDialog::connectTo()
{
  QString text = comboBox->currentText();

  try {
    saveViewerParameters(nullptr, text.toStdString().c_str());
  } catch (rfb::Exception& e) {
    vlog.error("%s", e.str());
    QMessageBox* dlg = new QMessageBox(QMessageBox::Critical,
                                       _("Error saving configuration"),
                                       QString::asprintf(_("Unable to save the default configuration:\n\n%s"), e.str()),
                                       QMessageBox::Close);
    AppManager::instance()->openDialog(dlg);
  }

  serverHistory.removeOne(text);
  serverHistory.push_front(text);

  try {
    saveServerHistory();
  } catch (rfb::Exception& e) {
    vlog.error("%s", e.str());
    QMessageBox* dlg = new QMessageBox(QMessageBox::Critical,
                                       _("Error loading server history"),
                                       QString::asprintf(_("Unable to save the server history:\n\n%s"), e.str()),
                                       QMessageBox::Close);
    AppManager::instance()->openDialog(dlg);
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
  about_vncviewer(this);
}

void ServerDialog::openLoadConfigDialog()
{
  QString filename = QFileDialog::getOpenFileName(this,
                                                  _("Select a TigerVNC configuration file"),
                                                  {},
                                                  _("TigerVNC configuration (*.tigervnc);;All files (*)"));
  if (!filename.isEmpty()) {
    try {
      QString server = loadViewerParameters(filename.toStdString().c_str());
      comboBox->setCurrentText(server);
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
      saveViewerParameters(filename.toStdString().c_str(),
                           comboBox->currentText().toStdString().c_str());
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

void ServerDialog::loadServerHistory()
{
  serverHistory.clear();

#ifdef _WIN32
  std::list<std::string> history;
  history = ::loadHistoryFromRegKey();
  for (auto const& s : history)
    serverHistory.push_back(s.c_str());
  return;
#endif

  const char* homeDir = os::getvncconfigdir();
  if (homeDir == nullptr)
    throw rdr::Exception("%s", _("Could not obtain the home directory path"));

  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", homeDir, SERVER_HISTORY);

  /* Read server history from file */
  FILE* f = fopen(filepath, "r");
  if (!f) {
    if (errno == ENOENT) {
      // no history file
      return;
    }
    throw rdr::Exception(_("Could not open \"%s\": %s"), filepath, strerror(errno));
  }

  int lineNr = 0;
  while (!feof(f)) {
    char line[256];

    // Read the next line
    lineNr++;
    if (!fgets(line, sizeof(line), f)) {
      if (feof(f))
        break;

      fclose(f);
      throw rdr::Exception(_("Failed to read line %d in file %s: %s"), lineNr, filepath, strerror(errno));
    }

    int len = strlen(line);

    if (len == (sizeof(line) - 1)) {
      fclose(f);
      throw rdr::Exception(_("Failed to read line %d in file %s: %s"), lineNr, filepath, _("Line too long"));
    }

    if ((len > 0) && (line[len - 1] == '\n')) {
      line[len - 1] = '\0';
      len--;
    }
    if ((len > 0) && (line[len - 1] == '\r')) {
      line[len - 1] = '\0';
      len--;
    }

    if (len == 0)
      continue;

    serverHistory.push_back(line);
  }

  fclose(f);
}

void ServerDialog::saveServerHistory()
{
#ifdef _WIN32
  std::list<std::string> history;
  for (auto const& s : qAsConst(serverHistory))
    history.push_back(s.toStdString());
  ::saveHistoryToRegKey(history);
#else
  const char* homeDir = os::getvncconfigdir();
  if (homeDir == nullptr) {
    throw rdr::Exception("%s", _("Could not obtain the home directory path"));
  }
  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", homeDir, SERVER_HISTORY);

  /* Write server history to file */
  FILE* f = fopen(filepath, "w");
  if (!f) {
    throw rdr::Exception(_("Could not open \"%s\": %s"), filepath, strerror(errno));
  }
  QTextStream stream(f, QIODevice::WriteOnly | QIODevice::WriteOnly);

  // Save the last X elements to the config file.
  for (int i = 0; i < serverHistory.size() && i <= SERVER_HISTORY_SIZE; i++) {
    stream << serverHistory[i] << "\n";
  }
  stream.flush();
  fclose(f);
#endif
}
