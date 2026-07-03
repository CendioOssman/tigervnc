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

// FIXME: Workaround for FLTK including windows.h
#ifdef WIN32
#include <winsock2.h>
#endif

#include <QFileDialog>
#include <QMessageBox>

// Conflicts with Qt
#define QPoint _FLTK_QPoint
#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_File_Chooser.H>
#undef QPoint

#include <core/Exception.h>
#include <core/LogWriter.h>
#include <core/i18n.h>
#include <core/string.h>
#include <core/xdgdirs.h>

#include <network/TcpSocket.h>

#include "fltk/layout.h"
#include "fltk/util.h"
#include "fltk/Fl_Suggestion_Input.h"
#include "ServerDialog.h"
#include "OptionsDialog.h"
#include "vncviewer.h"
#include "parameters.h"

static core::LogWriter vlog("ServerDialog");

const char* SERVER_HISTORY="tigervnc.history";

ServerDialog::ServerDialog()
  : Fl_Window(450, 0, "TigerVNC"),
  serverName(nullptr), fileChooser(nullptr),
  saveConflictDialog(nullptr),
  finishedCallback(nullptr), finishedUserData(nullptr)
{
  int x, y, x2;
  Fl_Button *button;
  Fl_Box *divider;

  result_ = 0;

  x = OUTER_MARGIN;
  y = OUTER_MARGIN;

  serverName = new Fl_Suggestion_Input(
    LBLLEFT(x, y, w() - OUTER_MARGIN*2, INPUT_HEIGHT, _("VNC server:")), {}
  );
  serverName->call_on_remove(
    [](Fl_Widget*, std::string s, void* data) {
      ((ServerDialog*)data)->onServerHistoryRemove(s);
    },
    this);
  serverName->call_to_normalize(serverHistoryNormalize);

  y += INPUT_HEIGHT + INNER_MARGIN;

  x2 = x;

  button = new Fl_Button(x2, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Options..."));
  button->callback(
    [](Fl_Widget*, void*) {
      OptionsDialog* dlg;

      dlg = new OptionsDialog();
      dlg->set_modal();
      dlg->finished([](Fl_Widget* d, void*) { Fl::delete_widget(d); });
      dlg->show();
    }, this);
  x2 += BUTTON_WIDTH + INNER_MARGIN;

  button = new Fl_Button(x2, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Load..."));
  button->callback(
    [](Fl_Widget*, void* data) {
      ((ServerDialog*)data)->handleLoad();
    },
    this);
  x2 += BUTTON_WIDTH + INNER_MARGIN;

  button = new Fl_Button(x2, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Save as..."));
  button->callback(
    [](Fl_Widget*, void* data) {
      ((ServerDialog*)data)->handleSaveAs();
    },
    this);
  x2 += BUTTON_WIDTH + INNER_MARGIN;

  y += BUTTON_HEIGHT + INNER_MARGIN;

  divider = new Fl_Box(0, y, w(), 2);
  divider->box(FL_THIN_DOWN_FRAME);

  y += divider->h() + INNER_MARGIN;

  // Symmetric margin around bottom button bar
  y += OUTER_MARGIN - INNER_MARGIN;

  button = new Fl_Button(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("About..."));
  button->callback([](Fl_Widget*, void*) { about_vncviewer(); }, this);

  x2 = w() - OUTER_MARGIN - BUTTON_WIDTH*2 - INNER_MARGIN*1;

  button = new Fl_Button(x2, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Cancel"));
  button->callback(
    [](Fl_Widget*, void* data) {
      ((ServerDialog*)data)->handleCancel();
    },
    this);
  x2 += BUTTON_WIDTH + INNER_MARGIN;

  button = new Fl_Return_Button(x2, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Connect"));
  button->callback(
    [](Fl_Widget*, void* data) {
      ((ServerDialog*)data)->handleConnect();
    },
    this);
  x2 += BUTTON_WIDTH + INNER_MARGIN;

  y += BUTTON_HEIGHT + INNER_MARGIN;

  /* Needed for resize to work sanely */
  resizable(nullptr);
  h(y-INNER_MARGIN+OUTER_MARGIN);

  callback(
    [](Fl_Widget*, void* data) {
      ((ServerDialog*)data)->handleCancel();
    }, this);

  try {
    loadServerHistory();
    serverName->set_suggestions(serverHistory);
  } catch (std::exception& e) {
    vlog.error(_("Unable to load the server history: %s"), e.what());
  }
}


ServerDialog::~ServerDialog()
{
  delete fileChooser;
  delete saveConflictDialog;
}


void ServerDialog::finished(Fl_Callback* cb, void* p)
{
  finishedCallback = cb;
  finishedUserData = p;
}


void ServerDialog::hide()
{
  Fl_Window::hide();

  if (finishedCallback != nullptr)
    finishedCallback(this, finishedUserData);
}


int ServerDialog::result()
{
  return result_;
}


std::string ServerDialog::getServerName()
{
  return serverName->value();
}


void ServerDialog::setServerName(const char* servername)
{
  serverName->value(servername);
}


void ServerDialog::handleLoad()
{
  delete fileChooser;

  fileChooser = new QFileDialog;
  fileChooser->setWindowTitle(_("Select a TigerVNC configuration file"));
  fileChooser->setDirectory(core::getuserhomedir());
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
    serverName->value(servername.c_str());
  } catch (std::exception& e) {
    QMessageBox* dlg;
    std::string msg;

    vlog.error("%s", e.what());

    dlg = new QMessageBox;
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
  delete fileChooser;

  fileChooser = new QFileDialog;
  fileChooser->setWindowTitle(_("Save the TigerVNC configuration to file"));
  fileChooser->setDirectory(core::getuserhomedir());
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
    std::string msg;

    delete saveConflictDialog;
    saveConflictDialog = new QMessageBox;
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
  const char* servername = serverName->value();

  try {
    saveViewerParameters(filename.toStdString().c_str(), servername);
  } catch (std::exception& e) {
    QMessageBox* dlg;
    std::string msg;

    vlog.error("%s", e.what());

    dlg = new QMessageBox;
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


void ServerDialog::handleCancel()
{
  result_ = 0;
  hide();
}


void ServerDialog::handleConnect()
{
  const char* servername = serverName->value();

  result_ = 1;

  try {
    saveViewerParameters(nullptr, servername);
  } catch (std::exception& e) {
    vlog.error(_("Unable to save the default configuration: %s"),
               e.what());
  }

  // avoid duplicates in the history
  serverHistory.remove(servername);
  serverHistory.insert(serverHistory.begin(), servername);

  try {
    saveServerHistory();
  } catch (std::exception& e) {
    vlog.error(_("Unable to save the server history: %s"), e.what());
  }

  hide();
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
