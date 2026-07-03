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

#include <string>
#include <list>

#include <QDialog>

class QComboBox;
class QString;

class ServerDialog : public QDialog {
  Q_OBJECT

public:
  ServerDialog(QWidget* parent=nullptr);

  std::string getServerName();
  void setServerName(const char* servername);

protected:
  void handleLoad();
  void handleLoadSelected(const QString& filename);
  void handleSaveAs();
  void handleSaveAsSelected(const QString& filename);
  void finishSaveAs(const QString& filename);
  void handleConnect();

private:
  void loadServerHistory();
  void saveServerHistory();

protected:
  QComboBox* serverName;
  std::list<std::string> serverHistory;
};

#endif
