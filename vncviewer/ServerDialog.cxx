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
#include <libgen.h>

#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_File_Chooser.H>

#include <os/os.h>
#include <rfb/Exception.h>
#include <rfb/Hostname.h>
#include <rfb/LogWriter.h>
#include <rfb/util.h>

#include "fltk/Fl_Message_Box.h"
#include "fltk/layout.h"
#include "fltk/util.h"
#include "ServerDialog.h"
#include "OptionsDialog.h"
#include "i18n.h"
#include "vncviewer.h"
#include "parameters.h"

static rfb::LogWriter vlog("ServerDialog");

const char* SERVER_HISTORY="tigervnc.history";

ServerDialog::ServerDialog()
  : Fl_Window(450, 0, _("VNC Viewer: Connection Details")),
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

  serverName = new Fl_Input_Choice(LBLLEFT(x, y, w() - OUTER_MARGIN*2,
                                   INPUT_HEIGHT, _("VNC server:")));
  // Bug fix for wrong background
  serverName->color(FL_BACKGROUND2_COLOR);
  y += INPUT_HEIGHT + INNER_MARGIN;

  x2 = x;

  button = new Fl_Button(x2, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Options..."));
  button->callback(
    [](Fl_Widget*, void*) { OptionsDialog::showDialog(); }, this);
  x2 += BUTTON_WIDTH + INNER_MARGIN;

  button = new Fl_Button(x2, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Load..."));
  button->callback(
    [](Fl_Widget*, void* data) {
      ((ServerDialog*)data)->handleLoad();
    },
    this);
  x2 += BUTTON_WIDTH + INNER_MARGIN;

  button = new Fl_Button(x2, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Save As..."));
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
    for (const std::string& entry : serverHistory)
      fltk_menu_add(serverName->menubutton(),
                    entry.c_str(), 0, nullptr);
  } catch (rfb::Exception& e) {
    vlog.error(_("Unable to load the server history: %s"), e.str());
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
  if (usedDir.empty())
    usedDir = os::getuserhomedir();

  delete fileChooser;

  fileChooser = new Fl_File_Chooser(usedDir.c_str(),
                                    _("TigerVNC configuration (*.tigervnc)"),
                                    0, _("Select a TigerVNC configuration file"));
  fileChooser->preview(0);
  fileChooser->previewButton->hide();
  // Fl_File_Chooser is buggy and calls the callback before hiding
  // when closing on Fl_Enter, hence this convoluted handling
  fileChooser->callback(
    [](Fl_File_Chooser*, void* data_) {
      Fl::add_timeout(
        0,
        [](void* data__) {
          ServerDialog* dialog_ = (ServerDialog*)data__;
          if (!dialog_->fileChooser->shown())
            dialog_->handleLoadSelected();
        },
        data_);
    },
    this);
  fileChooser->show();
}

void ServerDialog::handleLoadSelected()
{
  const char* filename = fileChooser->value();
  updateUsedDir(filename);

  try {
    serverName->value(loadViewerParameters(filename).c_str());
  } catch (rdr::Exception& e) {
    Fl_Alert_Box* dlg;

    vlog.error("%s", e.str());

    dlg = new Fl_Alert_Box(_("Error"),
                           _("Unable to load the specified "
                             "configuration file:\n\n"
                             "%s"), e.str());
    dlg->set_modal();
    dlg->finished([](Fl_Widget* d, void*) { Fl::delete_widget(d); });
    dlg->show();
  }
}


void ServerDialog::handleSaveAs()
{ 
  if (usedDir.empty())
    usedDir = os::getuserhomedir();

  delete fileChooser;

  fileChooser = new Fl_File_Chooser(usedDir.c_str(),
                                    _("TigerVNC configuration (*.tigervnc)"),
                                    2, _("Save the TigerVNC configuration to file"));
  
  fileChooser->preview(0);
  fileChooser->previewButton->hide();
  // Fl_File_Chooser is buggy and calls the callback before hiding
  // when closing on Fl_Enter, hence this convoluted handling
  fileChooser->callback(
    [](Fl_File_Chooser*, void* data_) {
      Fl::add_timeout(
        0,
        [](void* data__) {
          ServerDialog* dialog_ = (ServerDialog*)data__;
          if (!dialog_->fileChooser->shown())
            dialog_->handleSaveAsSelected();
        },
        data_);
    },
    this);
  fileChooser->show();
}

void ServerDialog::handleSaveAsSelected()
{
  const char* filename = fileChooser->value();
  updateUsedDir(filename);

  FILE* f = fopen(filename, "r");
  if (f) {
    // The file already exists.
    fclose(f);

    saveConflictDialog =
      new Fl_Choice_Box(_("File already exists"),
                        _("%s already exists. Do you want to "
                          "overwrite?"), nullptr, _("No"),
                        _("Overwrite"), filename);
    saveConflictDialog->set_modal();
    saveConflictDialog->finished(
      [](Fl_Widget*, void* data) {
        ((ServerDialog*)data)->handleSaveConflict();
      },
      this);
    saveConflictDialog->show();
    return;
  }

  finishSaveAs();
}


void ServerDialog::handleSaveConflict()
{
  Fl::delete_widget(saveConflictDialog);

  if (saveConflictDialog->result() != 2) {
    // If the user doesn't want to overwrite:
    fileChooser->show();
  } else {
    finishSaveAs();
  }
}


void ServerDialog::finishSaveAs()
{
  const char* servername = serverName->value();
  const char* filename = fileChooser->value();

  try {
    saveViewerParameters(filename, servername);
  } catch (rfb::Exception& e) {
    Fl_Alert_Box* dlg;

    vlog.error("%s", e.str());

    dlg = new Fl_Alert_Box(_("Error"),
                           _("Unable to save the specified "
                             "configuration file:\n\n"
                             "%s"), e.str());
    dlg->set_modal();
    dlg->finished([](Fl_Widget* d, void*) { Fl::delete_widget(d); });
    dlg->show();
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
  } catch (rfb::Exception& e) {
    vlog.error(_("Unable to save the default configuration: %s"), e.str());
  }

  // avoid duplicates in the history
  serverHistory.remove(servername);
  serverHistory.insert(serverHistory.begin(), servername);

  try {
    saveServerHistory();
  } catch (rfb::Exception& e) {
    vlog.error(_("Unable to save the server history: %s"), e.str());
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

void ServerDialog::updateUsedDir(const char* filename)
{
  char * name = strdup(filename);
  usedDir = dirname(name);
  free(name);
}
