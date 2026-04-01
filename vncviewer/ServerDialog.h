/* Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#ifndef __SERVERDIALOG_H__
#define __SERVERDIALOG_H__

#include <FL/Fl_Window.H>
#include <string>
#include <list>

class Fl_Widget;
class Fl_Choice_Box;
class Fl_File_Chooser;
class Fl_Input_Choice;

class ServerDialog : public Fl_Window {
public:
  ServerDialog();
  ~ServerDialog();

  void finished(Fl_Callback* cb, void* p=nullptr);

  void hide() override;

  int result();

  std::string getServerName();
  void setServerName(const char* servername);

protected:
  void handleLoad();
  void handleLoadSelected();
  void handleSaveAs();
  void handleSaveAsSelected();
  void handleSaveConflict();
  void finishSaveAs();
  void handleCancel();
  void handleConnect();

private:
  void loadServerHistory();
  void saveServerHistory();
  void updateUsedDir(const char* filename);

protected:
  Fl_Input_Choice *serverName;
  Fl_File_Chooser* fileChooser;
  Fl_Choice_Box* saveConflictDialog;
  std::list<std::string> serverHistory;
  std::string usedDir;

private:
  int result_;

  Fl_Callback* finishedCallback;
  void* finishedUserData;
};

#endif
