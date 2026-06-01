/* Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright 2012 Samuel Mannehed <samuel@cendio.se> for Cendio AB
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <algorithm>

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

#include <os/os.h>
#include <rfb/Exception.h>
#include <rfb/Hostname.h>
#include <rfb/LogWriter.h>
#include <rfb/util.h>

#include "ServerDialog.h"
#include "OptionsDialog.h"
#include "i18n.h"
#include "vncviewer.h"
#include "parameters.h"

static rfb::LogWriter vlog("ServerDialog");

const char* SERVER_HISTORY="tigervnc.history";

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
  serverName = new QComboBox;
  serverName->setEditable(true);
  serverName->setFocus();
  row1->addWidget(serverName, 1);
  layout->addLayout(row1);

  QHBoxLayout* row2 = new QHBoxLayout;
  QPushButton* optionsBtn = new QPushButton(_("Options..."));
  connect(optionsBtn, &QPushButton::clicked, this, [this]() {
    OptionsDialog* dlg = new OptionsDialog(isFullScreen(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
  });
  row2->addWidget(optionsBtn);
  QPushButton* loadBtn = new QPushButton(_("Load..."));
  connect(loadBtn, &QPushButton::clicked, this,
          &ServerDialog::handleLoad);
  row2->addWidget(loadBtn);
  QPushButton* saveAsBtn = new QPushButton(_("Save As..."));
  connect(saveAsBtn, &QPushButton::clicked, this,
          &ServerDialog::handleSaveAs);
  row2->addWidget(saveAsBtn);
  layout->addLayout(row2);

  QHBoxLayout* row3 = new QHBoxLayout;
  QPushButton* aboutBtn = new QPushButton(_("About..."));
  connect(aboutBtn, &QPushButton::clicked, this,
          [this]() { about_vncviewer(this); });
  row3->addWidget(aboutBtn);
  QPushButton* cancelBtn = new QPushButton(_("Cancel"));
  connect(cancelBtn, &QPushButton::clicked, this,
          &ServerDialog::reject);
  row3->addWidget(cancelBtn);
  QPushButton* connectBtn = new QPushButton(_("Connect"));
  connect(connectBtn, &QPushButton::clicked, this,
          &ServerDialog::handleConnect);
  row3->addWidget(connectBtn);
  layout->addLayout(row3);

  setLayout(layout);
  adjustSize();

  connect(serverName->lineEdit(), &QLineEdit::returnPressed, this,
          &ServerDialog::handleConnect);

  try {
    loadServerHistory();
  } catch (rdr::Exception& e) {
    vlog.error(_("Unable to load the server history: %s"), e.str());
  }

  QStringList strList;
  for (const std::string& s : serverHistory)
    strList.append(s.c_str());
  QStringListModel* model = new QStringListModel();
  model->setStringList(strList);
  serverName->setModel(model);
}

std::string ServerDialog::getServerName()
{
  return serverName->currentText().toStdString();
}

void ServerDialog::setServerName(const char* servername)
{
  serverName->setCurrentText(servername);
}

void ServerDialog::handleLoad()
{
  fileChooser = new QFileDialog(
    this, _("Select a TigerVNC configuration file"),
    os::getuserhomedir(),
    rfb::format("%s;;%s", _("TigerVNC configuration (*.tigervnc)"),
                _("All files (*)")).c_str());

  fileChooser->setFileMode(QFileDialog::ExistingFile);
  fileChooser->setModal(true);

  connect(fileChooser, &QFileDialog::fileSelected, this,
          &ServerDialog::handleLoadSelected);

  fileChooser->setAttribute(Qt::WA_DeleteOnClose);
  fileChooser->show();
}

void ServerDialog::handleLoadSelected(const QString& file)
{
  try {
    QString server = loadViewerParameters(file.toStdString().c_str());
    serverName->setCurrentText(server);
  } catch (rfb::Exception& e) {
    std::string msg;
    QMessageBox* dlg;

    vlog.error("%s", e.str());

    msg = rfb::format(_("Unable to load the specified configuration "
                        "file:\n\n%s"), e.str());
    dlg = new QMessageBox(QMessageBox::Critical, _("Error"),
                          msg.c_str(), QMessageBox::Ok, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
  }
}

void ServerDialog::handleSaveAs()
{
  fileChooser = new QFileDialog(this,
                                _("Save the TigerVNC configuration to file"),
                                os::getuserhomedir(),
                                _("TigerVNC configuration (*.tigervnc);;All files (*)"));
  fileChooser->setAcceptMode(QFileDialog::AcceptSave);
  // FIXME: Remove this flag and our custom handling
  fileChooser->setOptions(QFileDialog::DontConfirmOverwrite);
  fileChooser->setModal(true);

  connect(fileChooser, &QFileDialog::fileSelected, this,
          &ServerDialog::handleSaveAsSelected);

  fileChooser->setAttribute(Qt::WA_DeleteOnClose);
  fileChooser->show();
}

void ServerDialog::handleSaveAsSelected(const QString& file)
{
  if (QFile::exists(file)) {
    // The file already exists.
    std::string msg;
    QMessageBox* saveConflictDialog;

    saveConflictDialog = new QMessageBox(this);
    saveConflictDialog->setWindowTitle(_("File already exists"));
    saveConflictDialog->addButton(_("Overwrite"),
                                  QMessageBox::AcceptRole);
    saveConflictDialog->setDefaultButton(
      saveConflictDialog->addButton(_("No"), QMessageBox::RejectRole));
    msg = rfb::format(_("%s already exists. Do you want to overwrite?"),
                      file.toStdString().c_str());
    saveConflictDialog->setText(msg.c_str());
    saveConflictDialog->setModal(true);
    connect(saveConflictDialog, &QMessageBox::accepted, this,
            [this, file]() { finishSaveAs(file); });
    connect(saveConflictDialog, &QMessageBox::rejected, this,
            [this]() { handleSaveAs(); });
    saveConflictDialog->setAttribute(Qt::WA_DeleteOnClose);
    saveConflictDialog->show();
    return;
  }

  finishSaveAs(file);
}

void ServerDialog::finishSaveAs(const QString& file)
{
  try {
    std::string servername = serverName->currentText().toStdString();
    saveViewerParameters(file.toStdString().c_str(),
                         servername.c_str());
  } catch (rfb::Exception& e) {
    std::string msg;
    QMessageBox* dlg;

    vlog.error("%s", e.str());

    msg = rfb::format(_("Unable to save the specified configuration "
                        "file:\n\n%s"), e.str());
    dlg = new QMessageBox(QMessageBox::Critical,
                          _("Error"),
                          msg.c_str(),
                          QMessageBox::Ok,
                          this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
  }
}


void ServerDialog::handleConnect()
{
  std::string servername = serverName->currentText().toStdString();

  try {
    saveViewerParameters(nullptr, servername.c_str());
  } catch (rfb::Exception& e) {
    vlog.error(_("Unable to save the default configuration: %s"), e.str());
  }

  // avoid duplicates in the history
  serverHistory.remove(servername.c_str());
  serverHistory.insert(serverHistory.begin(), servername.c_str());

  try {
    saveServerHistory();
  } catch (rfb::Exception& e) {
    vlog.error(_("Unable to save the server history: %s"), e.str());
  }

  accept();
}


static bool same_server(const std::string& a, const std::string& b)
{
  std::string hostA, hostB;
  int portA, portB;

#ifndef WIN32
  if ((a.find("/") != std::string::npos) || (b.find("/") != std::string::npos))
    return a == b;
#endif

  try {
    rfb::getHostAndPort(a.c_str(), &hostA, &portA);
    rfb::getHostAndPort(b.c_str(), &hostB, &portB);
  } catch (rfb::Exception& e) {
    return false;
  }

  if (hostA != hostB)
    return false;

  if (portA != portB)
    return false;

  return true;
}


void ServerDialog::loadServerHistory()
{
  std::list<std::string> rawHistory;

  serverHistory.clear();

#ifdef _WIN32
  rawHistory = loadHistoryFromRegKey();
#else

  const char* stateDir = os::getvncstatedir();
  if (stateDir == nullptr)
    throw rdr::Exception(_("Could not determine VNC state directory path"));

  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", stateDir, SERVER_HISTORY);

  /* Read server history from file */
  FILE* f = fopen(filepath, "r");
  if (!f) {
    if (errno == ENOENT) {
      // no history file
      return;
    }
    std::string msg = rfb::format(_("Could not open \"%s\""), filepath);
    throw rdr::SystemException(msg.c_str(), errno);
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
      std::string msg = rfb::format(_("Failed to read line %d in "
                                      "file \"%s\""), lineNr, filepath);
      throw rdr::SystemException(msg.c_str(), errno);
    }

    int len = strlen(line);

    if (len == (sizeof(line) - 1)) {
      fclose(f);
      throw rdr::Exception(_("Failed to read line %d in file %s: %s"),
                           lineNr, filepath, _("Line too long"));
    }

    if ((len > 0) && (line[len-1] == '\n')) {
      line[len-1] = '\0';
      len--;
    }
    if ((len > 0) && (line[len-1] == '\r')) {
      line[len-1] = '\0';
      len--;
    }

    if (len == 0)
      continue;

    rawHistory.push_back(line);
  }

  fclose(f);
#endif

  // Filter out duplicates, even if they have different formats
  for (const std::string& entry : rawHistory) {
    if (std::find_if(serverHistory.begin(), serverHistory.end(),
                     [&entry](const std::string& s) { return same_server(s, entry); }) != serverHistory.end())
      continue;
    serverHistory.push_back(entry);
  }
}

void ServerDialog::saveServerHistory()
{
#ifdef _WIN32
  saveHistoryToRegKey(serverHistory);
  return;
#endif
  const char* stateDir = os::getvncstatedir();
  if (stateDir == nullptr)
    throw rdr::Exception(_("Could not determine VNC state directory path"));

  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", stateDir, SERVER_HISTORY);

  /* Write server history to file */
  FILE* f = fopen(filepath, "w+");
  if (!f) {
    std::string msg = rfb::format(_("Could not open \"%s\""), filepath);
    throw rdr::SystemException(msg.c_str(), errno);
  }

  // Save the last X elements to the config file.
  size_t count = 0;
  for (const std::string& entry : serverHistory) {
    if (++count > SERVER_HISTORY_SIZE)
      break;
    fprintf(f, "%s\n", entry.c_str());
  }

  fclose(f);
}
