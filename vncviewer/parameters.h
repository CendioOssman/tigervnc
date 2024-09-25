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

#include <QObject>
#include <QHash>
#include <QList>
#include <QUrl>

class ViewerConfig : public QObject
{
  Q_OBJECT
  //
  Q_PROPERTY(QString desktopSize READ desktopSize WRITE setDesktopSize NOTIFY desktopSizeChanged)
  Q_PROPERTY(bool remoteResize READ remoteResize WRITE setRemoteResize NOTIFY remoteResizeChanged)
  Q_PROPERTY(QString geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)

  // Compression properties
  Q_PROPERTY(bool autoSelect READ autoSelect WRITE setAutoSelect NOTIFY autoSelectChanged)
  Q_PROPERTY(bool fullColour READ fullColour WRITE setFullColour NOTIFY fullColourChanged)
  Q_PROPERTY(int lowColourLevel READ lowColourLevel WRITE setLowColourLevel NOTIFY lowColourLevelChanged)
  Q_PROPERTY(QString preferredEncoding READ preferredEncoding WRITE setPreferredEncoding NOTIFY preferredEncodingChanged)
  Q_PROPERTY(int preferredEncodingNum READ preferredEncodingNum WRITE setPreferredEncodingNum NOTIFY preferredEncodingChanged)
  Q_PROPERTY(bool customCompressLevel READ customCompressLevel WRITE setCustomCompressLevel NOTIFY customCompressLevelChanged)
  Q_PROPERTY(int compressLevel READ compressLevel WRITE setCompressLevel NOTIFY compressLevelChanged)
  Q_PROPERTY(bool noJpeg READ noJpeg WRITE setNoJpeg NOTIFY noJpegChanged)
  Q_PROPERTY(int qualityLevel READ qualityLevel WRITE setQualityLevel NOTIFY qualityLevelChanged)
  // Security properties
  Q_PROPERTY(bool encNone READ encNone WRITE setEncNone NOTIFY encNoneChanged)
  Q_PROPERTY(bool encTLSAnon READ encTLSAnon WRITE setEncTLSAnon NOTIFY encTLSAnonChanged)
  Q_PROPERTY(bool encTLSX509 READ encTLSX509 WRITE setEncTLSX509 NOTIFY encTLSX509Changed)
  Q_PROPERTY(bool encAES READ encAES WRITE setEncAES NOTIFY encAESChanged)
  Q_PROPERTY(bool authNone READ authNone WRITE setAuthNone NOTIFY authNoneChanged)
  Q_PROPERTY(bool authVNC READ authVNC WRITE setAuthVNC NOTIFY authVNCChanged)
  Q_PROPERTY(bool authPlain READ authPlain WRITE setAuthPlain NOTIFY authPlainChanged)
  Q_PROPERTY(QString x509CA READ x509CA WRITE setX509CA NOTIFY x509CAChanged)
  Q_PROPERTY(QString x509CRL READ x509CRL WRITE setX509CRL NOTIFY x509CRLChanged)
  // Input properties
  Q_PROPERTY(bool viewOnly READ viewOnly WRITE setViewOnly NOTIFY viewOnlyChanged)
  Q_PROPERTY(bool emulateMiddleButton READ emulateMiddleButton WRITE setEmulateMiddleButton NOTIFY emulateMiddleButtonChanged)
  Q_PROPERTY(bool dotWhenNoCursor READ dotWhenNoCursor WRITE setDotWhenNoCursor NOTIFY dotWhenNoCursorChanged)
  Q_PROPERTY(bool fullscreenSystemKeys READ fullscreenSystemKeys WRITE setFullscreenSystemKeys NOTIFY fullscreenSystemKeysChanged)
  Q_PROPERTY(QString menuKey READ menuKey WRITE setMenuKey NOTIFY menuKeyChanged)
  Q_PROPERTY(int menuKeyIndex READ menuKeyIndex NOTIFY menuKeyIndexChanged)
  Q_PROPERTY(QStringList menuKeys READ menuKeys CONSTANT)
  Q_PROPERTY(bool acceptClipboard READ acceptClipboard WRITE setAcceptClipboard NOTIFY acceptClipboardChanged)
  Q_PROPERTY(bool sendClipboard READ sendClipboard WRITE setSendClipboard NOTIFY sendClipboardChanged)
  // Display properties
  Q_PROPERTY(bool fullScreen READ fullScreen WRITE setFullScreen NOTIFY fullScreenChanged)
  Q_PROPERTY(FullScreenMode fullScreenMode READ fullScreenMode WRITE setFullScreenMode NOTIFY fullScreenModeChanged)
  Q_PROPERTY(QList<int> selectedScreens READ selectedScreens WRITE setSelectedScreens NOTIFY selectedScreensChanged)
  // Miscellaneous properties
  Q_PROPERTY(bool shared READ shared WRITE setShared NOTIFY sharedChanged)
  Q_PROPERTY(bool reconnectOnError READ reconnectOnError WRITE setReconnectOnError NOTIFY reconnectOnErrorChanged)
  //
  Q_PROPERTY(bool listenModeEnabled READ listenModeEnabled CONSTANT)
  Q_PROPERTY(QStringList serverHistory READ serverHistory WRITE setServerHistory NOTIFY serverHistoryChanged)
  Q_PROPERTY(QString aboutText READ aboutText CONSTANT)
  Q_PROPERTY(QString serverName READ serverName CONSTANT)
  Q_PROPERTY(QString serverHost READ serverHost CONSTANT)
  Q_PROPERTY(int serverPort READ serverPort CONSTANT);
  Q_PROPERTY(QString gatewayHost READ gatewayHost CONSTANT)
  Q_PROPERTY(int gatewayLocalPort READ gatewayLocalPort CONSTANT)

public:
  const char* SERVER_HISTORY="tigervnc.history";
  static const int SERVER_PORT_OFFSET = 5900; // ??? 5500;
  enum FullScreenMode {
    FSCurrent,
    FSAll,
    FSSelected,
  };
  Q_ENUM(FullScreenMode)

  virtual ~ViewerConfig();
  static ViewerConfig *config() { return config_; };
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
  bool encNone() const { return encNone_; }
  void setEncNone(bool value);
  bool encTLSAnon() const { return encTLSAnon_; }
  void setEncTLSAnon(bool value);
  bool encTLSX509() const { return encTLSX509_; }
  void setEncTLSX509(bool value);
  bool encAES() const { return encAES_; }
  void setEncAES(bool value);
  bool authNone() const { return authNone_; }
  void setAuthNone(bool value);
  bool authVNC() const { return authVNC_; }
  void setAuthVNC(bool value);
  bool authPlain() const { return authPlain_; }
  void setAuthPlain(bool value);
  QString x509CA() const;
  void setX509CA(QString value);
  QString x509CRL() const;
  void setX509CRL(QString value);
  //
  bool viewOnly() const;
  void setViewOnly(bool value);
  bool emulateMiddleButton() const;
  void setEmulateMiddleButton(bool value);
  bool dotWhenNoCursor() const;
  void setDotWhenNoCursor(bool value);
  bool fullscreenSystemKeys() const;
  void setFullscreenSystemKeys(bool value);
  QString menuKey() const;
  void setMenuKey(QString value);
  int menuKeyIndex() const;
  QStringList menuKeys() const { return menuKeys_; }
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
  QStringList serverHistory() const { return serverHistory_; }
  void setServerHistory(QStringList history);
  QString serverName() const { return serverName_; }
  void usage();
  bool listenModeEnabled() const;
  QString serverHost() const { return serverHost_; }
  int serverPort() const { return serverPort_; }
  QString gatewayHost() const;
  int gatewayLocalPort() const { return gatewayLocalPort_; }
  void setAccessPoint(QString accessPoint);

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
  void saveViewerParameters(QString path, QString serverName) const;
  QString loadViewerParameters(QString path);
  void loadServerHistory();
  void saveServerHistory();
  void handleOptions(); // <- CConn::handleOptions()
  QString aboutText();

private:
  static ViewerConfig *config_;
  //
  QHash<int, bool> availableSecurityTypes_; // Each element is a pair of (availableSecurityId, userPreferenceToUseIt).
  QStringList menuKeys_;
  bool encNone_;
  bool encTLSAnon_;
  bool encTLSX509_;
  bool encAES_;
  bool authNone_;
  bool authVNC_;
  bool authPlain_;
  QStringList serverHistory_;
  QString serverName_;
  QString serverHost_;
  int serverPort_;
  int gatewayLocalPort_;
  char *messageDir_;
  ViewerConfig();
  bool potentiallyLoadConfigurationFile(QString vncServerName);
  QString getlocaledir();
  void initializeLogger();
  void parserServerName();
};

#if 0
#include <QStringList>
#include <rfb/Configuration.h>
#include "MonitorIndicesParameter.h"

#ifdef _WIN32
#include <list>
#include <string>
#endif

#define SERVER_HISTORY_SIZE 20


extern rfb::IntParameter pointerEventInterval;
extern rfb::BoolParameter emulateMiddleButton;
extern rfb::BoolParameter dotWhenNoCursor;

extern rfb::StringParameter passwordFile;

extern rfb::BoolParameter autoSelect;
extern rfb::BoolParameter fullColour;
extern rfb::AliasParameter fullColourAlias;
extern rfb::IntParameter lowColourLevel;
extern rfb::AliasParameter lowColourLevelAlias;
extern rfb::StringParameter preferredEncoding;
extern rfb::BoolParameter customCompressLevel;
extern rfb::IntParameter compressLevel;
extern rfb::BoolParameter noJpeg;
extern rfb::IntParameter qualityLevel;

extern rfb::BoolParameter maximize;
extern rfb::BoolParameter fullScreen;
extern rfb::StringParameter fullScreenMode;
extern rfb::BoolParameter fullScreenAllMonitors; // deprecated
extern MonitorIndicesParameter fullScreenSelectedMonitors;
extern rfb::StringParameter desktopSize;
extern rfb::StringParameter geometry;
extern rfb::BoolParameter remoteResize;

extern rfb::BoolParameter listenMode;

extern rfb::BoolParameter viewOnly;
extern rfb::BoolParameter shared;

extern rfb::BoolParameter acceptClipboard;
extern rfb::BoolParameter setPrimary;
extern rfb::BoolParameter sendClipboard;
#if !defined(WIN32) && !defined(__APPLE__)
extern rfb::BoolParameter sendPrimary;
extern rfb::StringParameter display;
#endif

extern rfb::StringParameter menuKey;

extern rfb::BoolParameter fullscreenSystemKeys;
extern rfb::BoolParameter alertOnFatalError;
extern rfb::BoolParameter reconnectOnError;

extern rfb::StringParameter via;

void saveViewerParameters(const char *filename, const char *servername=nullptr);
char* loadViewerParameters(const char *filename);

#ifdef _WIN32
void loadHistoryFromRegKey(QStringList& serverHistory);
void saveHistoryToRegKey(const QStringList& serverHistory);
#endif
#endif
#endif
