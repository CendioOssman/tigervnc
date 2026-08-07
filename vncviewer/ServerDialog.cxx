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
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStringListModel>
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
  QFormLayout* form;
  QBoxLayout* box;
  QPushButton* button;

  setWindowTitle(_("VNC Viewer: Connection Details"));

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

  try {
    loadServerHistory();
    for (const std::string& entry : serverHistory)
      serverName->addItem(entry.c_str());
  } catch (rfb::Exception& e) {
    vlog.error(_("Unable to load the server history: %s"), e.str());
  }
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
  fileChooser->setDirectory(os::getuserhomedir());
  fileChooser->setFileMode(QFileDialog::ExistingFile);
  fileChooser->setNameFilters({_("TigerVNC configuration (*.tigervnc)"),
                               _("All files (*)")});
  // Portals break in dual FLTK/Qt mode
#if !defined(WIN32) && !defined(__APPLE__)
  fileChooser->setOption(QFileDialog::DontUseNativeDialog);
#endif
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
  } catch (rdr::Exception& e) {
    QMessageBox* dlg;
    std::string msg;

    vlog.error("%s", e.str());

    dlg = new QMessageBox(this);
    dlg->setIcon(QMessageBox::Critical);
    dlg->setWindowTitle(_("Error"));
    msg = rfb::format(_("Unable to load the specified configuration "
                        "file:\n\n%s"), e.str());
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
  fileChooser->setDirectory(os::getuserhomedir());
  fileChooser->setAcceptMode(QFileDialog::AcceptSave);
  fileChooser->setNameFilters({_("TigerVNC configuration (*.tigervnc)"),
                               _("All files (*)")});
  // FIXME: Remove this flag and our custom handling
  fileChooser->setOptions(QFileDialog::DontConfirmOverwrite);
  // Portals break in dual FLTK/Qt mode
#if !defined(WIN32) && !defined(__APPLE__)
  fileChooser->setOption(QFileDialog::DontUseNativeDialog);
#endif
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
    msg = rfb::format(_("%s already exists. Do you want to overwrite?"),
                      filename.toStdString().c_str());
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
  } catch (rfb::Exception& e) {
    QMessageBox* dlg;
    std::string msg;

    vlog.error("%s", e.str());

    dlg = new QMessageBox(this);
    dlg->setIcon(QMessageBox::Critical);
    dlg->setWindowTitle(_("Error"));
    msg = rfb::format(_("Unable to save the specified configuration "
                        "file:\n\n%s"), e.str());
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
  if ((a.find("/") != std::string::npos) ||
      (b.find("/") != std::string::npos))
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
