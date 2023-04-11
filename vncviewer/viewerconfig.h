#ifndef VIEWERCONFIG_H
#define VIEWERCONFIG_H

#include <QHash>
#include <QList>
#include <QUrl>

class ViewerConfig : public QObject
{
  Q_OBJECT
  // Qt viewer specific properties
  Q_PROPERTY(bool openGLFBOenabled READ openGLFBOenabled WRITE setOpenGLFBOenabled NOTIFY openGLFBOenabledChanged)
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
  Q_PROPERTY(QStringList serverHistory READ serverHistory WRITE setServerHistory NOTIFY serverHistoryChanged)

public:
  const char* SERVER_HISTORY="tigervnc.history";
  enum FullScreenMode {
    FSCurrent,
    FSAll,
    FSSelected,
  };
  Q_ENUM(FullScreenMode)

  virtual ~ViewerConfig();
  static ViewerConfig *config() { return m_config; };
  static int initialize();
  //
  bool openGLFBOenabled() const { return m_openGLFBOenabled; }
  void setOpenGLFBOenabled(bool value);
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
  bool encNone() const { return m_encNone; }
  void setEncNone(bool value);
  bool encTLSAnon() const { return m_encTLSAnon; }
  void setEncTLSAnon(bool value);
  bool encTLSX509() const { return m_encTLSX509; }
  void setEncTLSX509(bool value);
  bool encAES() const { return m_encAES; }
  void setEncAES(bool value);
  bool authNone() const { return m_authNone; }
  void setAuthNone(bool value);
  bool authVNC() const { return m_authVNC; }
  void setAuthVNC(bool value);
  bool authPlain() const { return m_authPlain; }
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
  QStringList menuKeys() const { return m_menuKeys; }
  bool acceptClipboard() const;
  void setAcceptClipboard(bool value);
  bool sendClipboard() const;
  void setSendClipboard(bool value);
  //
  bool fullScreen() const;
  void setFullScreen(bool value);
  ViewerConfig::FullScreenMode fullScreenMode() const;
  void setFullScreenMode(ViewerConfig::FullScreenMode value);
  QList<int> selectedScreens() const; // { return m_selectedScreens; }
  void setSelectedScreens(QList<int> value);
  //
  bool shared() const;
  void setShared(bool value);
  bool reconnectOnError() const;
  void setReconnectOnError(bool value);
  //
  QStringList serverHistory() const { return m_serverHistory; }
  void setServerHistory(QStringList history);

signals:
  void openGLFBOenabledChanged(bool value);
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

public slots:
  bool installedSecurity(int type) const;
  bool enabledSecurity(int type) const;
  QString toLocalFile(const QUrl url) const;
  void saveViewerParameters(QString path, QString serverName) const;
  QString loadViewerParameters(QString path);
  void loadServerHistory();
  void saveServerHistory();
  void handleOptions(); // <- CConn::handleOptions()

private:
  static ViewerConfig *m_config;
  bool m_openGLFBOenabled;
  QHash<int, bool> m_availableSecurityTypes; // Each element is a pair of (availableSecurityId, userPreferenceToUseIt).
  QStringList m_menuKeys;
  bool m_encNone;
  bool m_encTLSAnon;
  bool m_encTLSX509;
  bool m_encAES;
  bool m_authNone;
  bool m_authVNC;
  bool m_authPlain;
  QStringList m_serverHistory;
  ViewerConfig();
};

#endif // VIEWERCONFIG_H
