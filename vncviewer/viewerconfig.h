/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
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
#ifndef __VIEWERCONFIG_H__
#define __VIEWERCONFIG_H__

#include <QHash>
#include <QList>
#include <QObject>
#include <QUrl>

class ViewerConfig : public QObject
{
  Q_OBJECT

public:
  enum FullscreenType
  {
    Current,
    All,
    Selected
  };

  static const int SERVER_PORT_OFFSET = 5900; // ??? 5500;

  static ViewerConfig* instance();

  static FullscreenType fullscreenType();
  static bool canFullScreenOnMultiDisplays();
  static bool hasWM();
  static void usage();

  void initialize();

  QString getServerName() const { return serverName; }

  QString getServerHost() const { return serverHost; }

  int getServerPort() const { return serverPort; }

  QString getGatewayHost() const;

  int getGatewayLocalPort() const { return gatewayLocalPort; }

  void setServer(QString name);

  QString getFinalAddress() const;

  void saveViewerParameters(QString path, QString name);
  QString loadViewerParameters(QString path);

signals:
  void errorOccurred(QString str);

private:
  ViewerConfig();

  QString serverName;
  QString serverHost;
  int serverPort = SERVER_PORT_OFFSET;
  int gatewayLocalPort = 0;

  bool potentiallyLoadConfigurationFile(QString vncServerName);
  void parseServerName();
};

#endif
