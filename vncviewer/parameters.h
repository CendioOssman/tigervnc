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
#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

#include <QHash>
#include <QList>
#include <QObject>
#include <QUrl>

class ViewerConfig : public QObject
{
  Q_OBJECT

public:
  const char* SERVER_HISTORY = "tigervnc.history";
  static const int SERVER_PORT_OFFSET = 5900; // ??? 5500;

  enum FullScreenMode {
    FSCurrent,
    FSAll,
    FSSelected,
  };
  Q_ENUM(FullScreenMode)

  virtual ~ViewerConfig();

  static ViewerConfig* config() { return config_; };

  static int initialize();
  //
  QString desktopSize() const;
  void setDesktopSize(QString value);
  bool remoteResize() const;
  void setRemoteResize(bool value);
  QString geometry() const;
  void setGeometry(QString value);
  //
  bool autoSelect() const;
  void setAutoSelect(bool value);
  bool fullColour() const;
  void setFullColour(bool value);
  int lowColourLevel() const;
  void setLowColourLevel(int value);
  QString preferredEncoding() const;
  void setPreferredEncoding(QString value);
  int preferredEncodingNum() const;
  void setPreferredEncodingNum(int value);
  bool customCompressLevel() const;
  void setCustomCompressLevel(bool value);
  int compressLevel() const;
  void setCompressLevel(int value);
  bool noJpeg() const;
  void setNoJpeg(bool value);
  int qualityLevel() const;
  void setQualityLevel(int value);

  //
  bool getEncNone() const { return encNone; }

  void setEncNone(bool value);

  bool getEncTLSAnon() const { return encTLSAnon; }

  void setEncTLSAnon(bool value);

  bool getEncTLSX509() const { return encTLSX509; }

  void setEncTLSX509(bool value);

  bool getEncAES() const { return encAES; }

  void setEncAES(bool value);

  bool getAuthNone() const { return authNone; }

  void setAuthNone(bool value);

  bool getAuthVNC() const { return authVNC; }

  void setAuthVNC(bool value);

  bool getAuthPlain() const { return authPlain; }

  void setAuthPlain(bool value);
  QString x509CA() const;
  void setX509CA(QString value);
  QString x509CRL() const;
  void setX509CRL(QString value);
  //
  bool viewOnly() const;
  void setViewOnly(bool value);
  int pointerEventInterval() const;
  void setPointerEventInterval(int value);
  bool emulateMiddleButton() const;
  void setEmulateMiddleButton(bool value);
  bool dotWhenNoCursor() const;
  void setDotWhenNoCursor(bool value);
  bool fullscreenSystemKeys() const;
  void setFullscreenSystemKeys(bool value);
  QString menuKey() const;
  void setMenuKey(QString value);
  int menuKeyIndex() const;

  QStringList getMenuKeys() const { return menuKeys; }

  bool acceptClipboard() const;
  void setAcceptClipboard(bool value);
  bool sendClipboard() const;
  void setSendClipboard(bool value);
  //
  bool fullScreen() const;
  void setFullScreen(bool value);
  ViewerConfig::FullScreenMode fullScreenMode() const;
  void setFullScreenMode(ViewerConfig::FullScreenMode value);
  QList<int> selectedScreens() const;
  void setSelectedScreens(QList<int> value);
  //
  bool shared() const;
  void setShared(bool value);
  bool reconnectOnError() const;
  void setReconnectOnError(bool value);

  //
  QStringList getServerHistory() const { return serverHistory; }

  void setServerHistory(QStringList history);
  void addToServerHistory(QString value);

  QString getServerName() const { return serverName; }

  void usage();
  bool listenModeEnabled() const;

  QString getServerHost() const { return serverHost; }

  int getServerPort() const { return serverPort; }

  QString gatewayHost() const;

  int getGatewayLocalPort() const { return gatewayLocalPort; }

  void setAccessPoint(QString accessPoint);

  //
  int qtVersionMajor() const { return QT_VERSION_MAJOR; }

  int qtVersionMinor() const { return QT_VERSION_MINOR; }

  bool haveGNUTLS() const
  {
#if defined(HAVE_GNUTLS)
    return true;
#else
    return false;
#endif
  }

  bool haveNETTLE() const
  {
#if defined(HAVE_NETTLE)
    return true;
#else
    return false;
#endif
  }

  bool haveH264() const
  {
#if defined(HAVE_H264)
    return true;
#else
    return false;
#endif
  }

  bool canFullScreenOnMultiDisplays() const;

signals:
  //
  void desktopSizeChanged(QString value);
  void remoteResizeChanged(bool value);
  void geometryChanged(QString value);
  //
  void autoSelectChanged(bool value);
  void fullColourChanged(bool value);
  void lowColourLevelChanged(int value);
  void preferredEncodingChanged(QString value);
  void customCompressLevelChanged(bool value);
  void compressLevelChanged(int value);
  void noJpegChanged(bool value);
  void qualityLevelChanged(int value);
  //
  void encNoneChanged(bool value);
  void encTLSAnonChanged(bool value);
  void encTLSX509Changed(bool value);
  void encAESChanged(bool value);
  void authNoneChanged(bool value);
  void authVNCChanged(bool value);
  void authPlainChanged(bool value);
  void x509CAChanged(QString path);
  void x509CRLChanged(QString path);
  //
  void viewOnlyChanged(bool value);
  void pointerEventIntervalChanged(int value);
  void emulateMiddleButtonChanged(bool value);
  void dotWhenNoCursorChanged(bool value);
  void fullscreenSystemKeysChanged(bool value);
  void menuKeyChanged(QString value);
  void menuKeyIndexChanged(QString value);
  void acceptClipboardChanged(bool value);
  void sendClipboardChanged(bool value);
  //
  void fullScreenChanged(bool value);
  void fullScreenModeChanged(ViewerConfig::FullScreenMode value);
  void selectedScreensChanged(QList<int> value);
  //
  void sharedChanged(bool value);
  void reconnectOnErrorChanged(bool value);
  //
  void serverHistoryChanged(QStringList history);
  void accessPointChanged(QString accessPoint);

public slots:
  bool installedSecurity(int type) const;
  bool enabledSecurity(int type) const;
  QString toLocalFile(const QUrl url) const;
  void saveViewerParameters(QString path, QString serverName);
  QString loadViewerParameters(QString path);
  void loadServerHistory();
  void saveServerHistory();
  void handleOptions(); // <- CConn::handleOptions()
  QString aboutText();

private:
  static ViewerConfig* config_;
  //
  QHash<int, bool> availableSecurityTypes; // Each element is a pair of (availableSecurityId, userPreferenceToUseIt).
  QStringList menuKeys;
  bool encNone;
  bool encTLSAnon;
  bool encTLSX509;
  bool encAES;
  bool authNone;
  bool authVNC;
  bool authPlain;
  QStringList serverHistory;
  QString serverName;
  QString serverHost;
  int serverPort;
  int gatewayLocalPort;
  char* messageDir;
  ViewerConfig();
  bool potentiallyLoadConfigurationFile(QString vncServerName);
  QString getlocaledir();
  void initializeLogger();
  void parseServerName();
};

#endif
