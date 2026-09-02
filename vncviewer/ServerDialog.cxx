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

#include <QBoxLayout>
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QMessageBox>

#include <core/Exception.h>
#include <core/LogWriter.h>
#include <core/i18n.h>
#include <core/string.h>
#include <core/xdgdirs.h>

#include <network/TcpSocket.h>

#include "ServerDialog.h"
#include "OptionsDialog.h"
#include "vncviewer.h"
#include "parameters.h"

static core::LogWriter vlog("ServerDialog");

const char* SERVER_HISTORY="tigervnc.history";

ServerDialog::ServerDialog(QWidget* parent)
  : QDialog(parent)
{
  QFormLayout* form;
  QBoxLayout* box;
  QPushButton* button;

  setWindowTitle("TigerVNC");
#ifdef __APPLE__
  setWindowFlag(Qt::CustomizeWindowHint, true);
  setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  setWindowFlag(Qt::WindowFullscreenButtonHint, false);
#endif

  QBoxLayout* layout = new QVBoxLayout;

  form = new QFormLayout;
  layout->addLayout(form);

  serverName = new QComboBox;
  serverName->setEditable(true);
  form->addRow(_("VNC server:"), serverName);

  box = new QHBoxLayout;
  layout->addLayout(box);

  button = new QPushButton(_("Options..."));
  connect(button, &QPushButton::clicked, this, [this]() {
    OptionsDialog* dlg;

    dlg = new OptionsDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
  });
  box->addWidget(button);

  button = new QPushButton(_("Load..."));
  connect(button, &QPushButton::clicked, this,
          &ServerDialog::handleLoad);
  box->addWidget(button);

  button = new QPushButton(_("Save As..."));
  connect(button, &QPushButton::clicked, this,
          &ServerDialog::handleSaveAs);
  box->addWidget(button);

  box->addStretch(1);

  layout->addStretch(1);

  QFrame* divider = new QFrame;
  divider->setFrameShape(QFrame::StyledPanel);
  divider->setFixedHeight(1);
  layout->addWidget(divider);

  box = new QHBoxLayout;
  layout->addLayout(box);

  button = new QPushButton(_("About..."));
  connect(button, &QPushButton::clicked, this,
          [this]() { about_vncviewer(this); });
  box->addWidget(button);

  box->addStretch(1);

  button = new QPushButton(_("Cancel"));
  connect(button, &QPushButton::clicked, this,
          &ServerDialog::reject);
  box->addWidget(button);

  button = new QPushButton(_("Connect"));
  button->setDefault(true);
  connect(button, &QPushButton::clicked, this,
          &ServerDialog::handleConnect);
  box->addWidget(button);

  setLayout(layout);
  adjustSize();

  QStringList strlist;

  try {
    loadServerHistory();
    for (const std::string& entry : serverHistory)
      strlist.append(entry.c_str());
  } catch (std::exception& e) {
    vlog.error(_("Unable to load the server history: %s"), e.what());
  }

  QCompleter* completer = new QCompleter(strlist, this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setFilterMode(Qt::MatchContains);
  serverName->addItems(strlist);
  serverName->setCompleter(completer);
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
  QFileDialog *fileChooser;

  fileChooser = new QFileDialog(this);
  fileChooser->setWindowTitle(_("Select a TigerVNC configuration file"));
  fileChooser->setDirectory(core::getuserhomedir());
  fileChooser->setFileMode(QFileDialog::ExistingFile);
  fileChooser->setNameFilters({_("TigerVNC configuration (*.tigervnc)"),
                               _("All files (*)")});
  QObject::connect(
    fileChooser, &QFileDialog::fileSelected,
    [this](const QString& f) { handleLoadSelected(f); });
  fileChooser->open();
}

void ServerDialog::handleLoadSelected(const QString& filename)
{
  try {
    std::string servername;
    servername = loadViewerParameters(filename.toStdString().c_str());
    serverName->setCurrentText(servername.c_str());
  } catch (std::exception& e) {
    QMessageBox* dlg;
    std::string msg;

    vlog.error("%s", e.what());

    dlg = new QMessageBox(this);
    dlg->setIcon(QMessageBox::Critical);
    dlg->setWindowTitle(_("Error"));
    msg = core::format(_("Unable to load the specified configuration "
                         "file:\n\n%s"), e.what());
    dlg->setText(msg.c_str());
    dlg->addButton(QMessageBox::Ok);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
  }
}


void ServerDialog::handleSaveAs()
{
  QFileDialog *fileChooser;

  fileChooser = new QFileDialog(this);
  fileChooser->setWindowTitle(_("Save the TigerVNC configuration to file"));
  fileChooser->setDirectory(core::getuserhomedir());
  fileChooser->setAcceptMode(QFileDialog::AcceptSave);
  fileChooser->setNameFilters({_("TigerVNC configuration (*.tigervnc)"),
                               _("All files (*)")});
  // FIXME: Remove this flag and our custom handling
  fileChooser->setOptions(QFileDialog::DontConfirmOverwrite);
  QObject::connect(
    fileChooser, &QFileDialog::fileSelected,
    [this](const QString& f) { handleSaveAsSelected(f); });
  fileChooser->open();
}

void ServerDialog::handleSaveAsSelected(const QString& filename)
{
  FILE* f = fopen(filename.toStdString().c_str(), "r");
  if (f) {
    // The file already exists.
    fclose(f);
    QMessageBox* saveConflictDialog;
    std::string msg;

    saveConflictDialog = new QMessageBox(this);
    saveConflictDialog->setIcon(QMessageBox::Warning);
    saveConflictDialog->setWindowTitle(_("File already exists"));
    msg = core::format(_("%s already exists. Do you want to "
                         "overwrite?"), filename.toStdString().c_str());
    saveConflictDialog->setText(msg.c_str());
    saveConflictDialog->addButton(_("Overwrite"),
                                  QMessageBox::AcceptRole);
    saveConflictDialog->addButton(QMessageBox::No);
    saveConflictDialog->setDefaultButton(QMessageBox::No);
    QObject::connect(saveConflictDialog, &QMessageBox::accepted,
                     [this, filename]() { finishSaveAs(filename); });
    QObject::connect(saveConflictDialog, &QMessageBox::rejected,
                     [this]() { handleSaveAs(); });
    saveConflictDialog->open();
    return;
  }

  finishSaveAs(filename);
}


void ServerDialog::finishSaveAs(const QString& filename)
{
  std::string servername = serverName->currentText().toStdString();

  try {
    saveViewerParameters(filename.toStdString().c_str(),
                         servername.c_str());
  } catch (std::exception& e) {
    QMessageBox* dlg;
    std::string msg;

    vlog.error("%s", e.what());

    dlg = new QMessageBox(this);
    dlg->setIcon(QMessageBox::Critical);
    dlg->setWindowTitle(_("Error"));
    msg = core::format(_("Unable to save the specified configuration "
                         "file:\n\n%s"), e.what());
    dlg->setText(msg.c_str());
    dlg->addButton(QMessageBox::Ok);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
  }
}


void ServerDialog::handleConnect()
{
  std::string servername = serverName->currentText().toStdString();

  try {
    saveViewerParameters(nullptr, servername.c_str());
  } catch (std::exception& e) {
    vlog.error(_("Unable to save the default configuration: %s"),
               e.what());
  }

  // avoid duplicates in the history
  serverHistory.remove(servername.c_str());
  serverHistory.insert(serverHistory.begin(), servername.c_str());

  try {
    saveServerHistory();
  } catch (std::exception& e) {
    vlog.error(_("Unable to save the server history: %s"), e.what());
  }

  accept();
}


static bool same_server(const std::string& a, const std::string& b)
{
  std::string hostA, hostB;
  int portA, portB;

#ifndef WIN32
  if ((a.find("/") != std::string::npos) ||
      (b.find("/") != std::string::npos))
    return a == b;
#endif

  try {
    network::getHostAndPort(a.c_str(), &hostA, &portA);
    network::getHostAndPort(b.c_str(), &hostB, &portB);
  } catch (std::exception& e) {
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

  const char* stateDir = core::getvncstatedir();
  if (stateDir == nullptr)
    throw std::runtime_error(_("Could not determine VNC state directory path"));

  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", stateDir, SERVER_HISTORY);

  /* Read server history from file */
  FILE* f = fopen(filepath, "r");
  if (!f) {
    if (errno == ENOENT) {
      // no history file
      return;
    }
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), filepath), errno);
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
      throw core::posix_error(
        core::format(_("Failed to read line %d in file \"%s\""),
                     lineNr, filepath),
        errno);
    }

    int len = strlen(line);

    if (len == (sizeof(line) - 1)) {
      fclose(f);
      std::string msg = core::format(_("Failed to read line %d in "
                                       "file \"%s\""),
                                     lineNr, filepath);
      throw std::runtime_error(
        core::format("%s: %s", msg.c_str(), _("Line too long")));
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
                     [&entry](const std::string& s) {
                       return same_server(s, entry);
                     }) != serverHistory.end())
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

  const char* stateDir = core::getvncstatedir();
  if (stateDir == nullptr)
    throw std::runtime_error(_("Could not determine VNC state directory path"));

  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", stateDir, SERVER_HISTORY);

  /* Write server history to file */
  FILE* f = fopen(filepath, "w+");
  if (!f) {
    std::string msg = core::format(_("Failed to open \"%s\""), filepath);
    throw core::posix_error(msg.c_str(), errno);
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

void ServerDialog::onServerHistoryRemove(std::string s)
{
  serverHistory.remove(s);
  saveServerHistory();
}

std::string ServerDialog::serverHistoryNormalize(const std::string s)
{
  // Convert to lowercase for case-insensitity
  std::string result = s;
  transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}
