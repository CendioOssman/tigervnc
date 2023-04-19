#include <QQmlEngine>
#include <QDir>
#include <QTextStream>
#include "viewerconfig.h"
#include "parameters.h"
#include "menukey.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "rfb/encodings.h"
#include "rfb/Security.h"
#include "rfb/SecurityClient.h"
#ifdef HAVE_GNUTLS
#include <rfb/CSecurityTLS.h>
#endif
#include "rdr/Exception.h"

extern int getvnchomedir(char **dirp);

ViewerConfig *ViewerConfig::m_config;

ViewerConfig::ViewerConfig()
    : QObject(nullptr)
    , m_openGLFBOenabled(QString(qgetenv("TIGERVNC_OPENGL_ENABLED")) == "1")
    , m_encNone(false)
    , m_encTLSAnon(false)
    , m_encTLSX509(false)
    , m_encAES(false)
    , m_authNone(false)
    , m_authVNC(false)
    , m_authPlain(false)
{
    loadViewerParameters(nullptr);

    rfb::Security security(rfb::SecurityClient::secTypes);
    auto secTypes = security.GetEnabledSecTypes();
    for (auto iter = secTypes.begin(); iter != secTypes.end(); ++iter) {
        switch (*iter) {
        case rfb::secTypeNone:
            m_encNone = true;
            m_authNone = true;
            break;
        case rfb::secTypeVncAuth:
            m_encNone = true;
            m_authVNC = true;
            break;
        }
    }
    auto secTypesExt = security.GetEnabledExtSecTypes();
    for (auto iterExt = secTypesExt.begin(); iterExt != secTypesExt.end(); ++iterExt) {
        switch (*iterExt) {
        case rfb::secTypePlain:
            m_encNone = true;
            m_authPlain = true;
            break;
        case rfb::secTypeTLSNone:
            m_encTLSAnon = true;
            m_authNone = true;
            break;
        case rfb::secTypeTLSVnc:
            m_encTLSAnon = true;
            m_authVNC = true;
            break;
        case rfb::secTypeTLSPlain:
            m_encTLSAnon = true;
            m_authPlain = true;
            break;
        case rfb::secTypeX509None:
            m_encTLSX509 = true;
            m_authNone = true;
            break;
        case rfb::secTypeX509Vnc:
            m_encTLSX509 = true;
            m_authVNC = true;
            break;
        case rfb::secTypeX509Plain:
            m_encTLSX509 = true;
            m_authPlain = true;
            break;
        case rfb::secTypeRA2:
        case rfb::secTypeRA256:
            m_encAES = true;
        case rfb::secTypeRA2ne:
        case rfb::secTypeRAne256:
            m_authVNC = true;
            m_authPlain = true;
            break;
        }
    }
    auto keysyms = getMenuKeySymbols();
    for (int i = 0; i < getMenuKeySymbolCount(); i++) {
        m_menuKeys.append(keysyms[i].name);
    }

    loadServerHistory();
}

ViewerConfig::~ViewerConfig()
{
}

int ViewerConfig::initialize()
{
    m_config = new ViewerConfig();
    qRegisterMetaType<ViewerConfig::FullScreenMode>("ViewerConfig::FullScreenMode");
    qmlRegisterSingletonType<ViewerConfig>("Qt.TigerVNC", 1, 0, "Config", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        return m_config;
    });
    return 0;
}

bool ViewerConfig::installedSecurity(int type) const
{
    return m_availableSecurityTypes.contains(type);
}

bool ViewerConfig::enabledSecurity(int type) const
{
    auto found = m_availableSecurityTypes.find(type);
    return found != m_availableSecurityTypes.end() && *found;
}

QString ViewerConfig::toLocalFile(const QUrl url) const
{
    return QDir::toNativeSeparators(url.toLocalFile());
}

void ViewerConfig::saveViewerParameters(QString path, QString serverName) const
{
    ::saveViewerParameters(path.isEmpty() ? nullptr : path.toStdString().c_str(), serverName.toStdString().c_str());
}

QString ViewerConfig::loadViewerParameters(QString path)
{
    return QString(::loadViewerParameters(path.trimmed().length() > 0 ? path.toStdString().c_str() : nullptr));
}

void ViewerConfig::loadServerHistory()
{
    m_serverHistory.clear();

  #ifdef _WIN32
    loadHistoryFromRegKey(m_serverHistory);
    return;
  #endif

    char* homeDir = NULL;
    if (getvnchomedir(&homeDir) == -1)
      throw rdr::Exception("%s", tr("Could not obtain the home directory path").toStdString().c_str());

    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s%s", homeDir, SERVER_HISTORY);
    delete[] homeDir;

    /* Read server history from file */
    FILE* f = fopen(filepath, "r");
    if (!f) {
      if (errno == ENOENT) {
        // no history file
        return;
      }
      throw rdr::Exception("%s", tr("Could not open \"%1\": %2").arg("%s", filepath).arg("%s", strerror(errno)).toStdString().c_str());
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
        throw rdr::Exception("%s", tr("Failed to read line %1 in file %2: %3").arg("%d", lineNr).arg("%s", filepath).arg("%s", strerror(errno)).toStdString().c_str());
      }

      int len = strlen(line);

      if (len == (sizeof(line) - 1)) {
        fclose(f);
        throw rdr::Exception("%s", tr("Failed to read line %1 in file %2: %3").arg("%d", lineNr).arg("%s", filepath).arg("%s", tr("Line too long")).toStdString().c_str());
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

      m_serverHistory.push_back(line);
    }

    fclose(f);
}

void ViewerConfig::setServerHistory(QStringList history)
{
    if (m_serverHistory != history) {
        m_serverHistory = history;
        saveServerHistory();
        emit serverHistoryChanged(history);
    }
}

void ViewerConfig::saveServerHistory()
{
#ifdef _WIN32
    saveHistoryToRegKey(m_serverHistory);
    return;
#endif

    char* homeDir = nullptr;
    if (getvnchomedir(&homeDir) == -1) {
        throw rdr::Exception("%s", tr("Could not obtain the home directory path").toStdString().c_str());
    }

    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s%s", homeDir, SERVER_HISTORY);
    delete[] homeDir;

    /* Write server history to file */
    FILE *f = fopen(filepath, "wb");
    if (!f) {
      throw rdr::Exception("%s", tr("Could not open \"%1\": %2").arg("%s", filepath).arg("%s", strerror(errno)).toStdString().c_str());
    }
    QTextStream stream(f, QIODevice::WriteOnly | QIODevice::Append);

    // Save the last X elements to the config file.
    for(int i = 0; i < m_serverHistory.size() && i <= SERVER_HISTORY_SIZE; i++) {
        stream << m_serverHistory[i] << "\n";
    }
    fclose(f);
}

void ViewerConfig::setOpenGLFBOenabled(bool value)
{
  if (m_openGLFBOenabled != value) {
    m_openGLFBOenabled = value;
    emit openGLFBOenabledChanged(value);
  }
}

bool ViewerConfig::autoSelect() const
{
    return ::autoSelect;
}

void ViewerConfig::setAutoSelect(bool value)
{
    if (::autoSelect != value) {
        ::autoSelect.setParam(value);
        emit autoSelectChanged(value);
    }
}

bool ViewerConfig::fullColour() const
{
    return ::fullColour;
}

void ViewerConfig::setFullColour(bool value)
{
    if (::fullColour != value) {
        ::fullColour.setParam(value);
        emit fullColourChanged(value);
    }
}

int ViewerConfig::lowColourLevel() const
{
    return ::lowColourLevel;
}

void ViewerConfig::setLowColourLevel(int value)
{
    if (::lowColourLevel != value) {
        ::lowColourLevel.setParam(value);
        emit lowColourLevelChanged(value);
    }
}

QString ViewerConfig::preferredEncoding() const
{
    return QString(::preferredEncoding);
}

void ViewerConfig::setPreferredEncoding(QString value)
{
    if (::preferredEncoding != value) {
        ::preferredEncoding.setParam(value.toStdString().c_str());
        emit preferredEncodingChanged(value);
    }
}

int ViewerConfig::preferredEncodingNum() const
{
    QString name = preferredEncoding().toLower();
    return rfb::encodingNum(name.toStdString().c_str());
}

void ViewerConfig::setPreferredEncodingNum(int value)
{
    QString name(rfb::encodingName(value));
    setPreferredEncoding(name);
}

bool ViewerConfig::customCompressLevel() const
{
    return ::customCompressLevel;
}

void ViewerConfig::setCustomCompressLevel(bool value)
{
    if (::customCompressLevel != value) {
        ::customCompressLevel.setParam(value);
        emit customCompressLevelChanged(value);
    }
}

int ViewerConfig::compressLevel() const
{
    return ::compressLevel;
}

void ViewerConfig::setCompressLevel(int value)
{
    if (::compressLevel != value) {
        ::compressLevel.setParam(value);
        emit compressLevelChanged(value);
    }
}

bool ViewerConfig::noJpeg() const
{
    return ::noJpeg;
}

void ViewerConfig::setNoJpeg(bool value)
{
    if (::noJpeg != value) {
        ::noJpeg.setParam(value);
        emit noJpegChanged(value);
    }
}

int ViewerConfig::qualityLevel() const
{
    return ::qualityLevel;
}

void ViewerConfig::setQualityLevel(int value)
{
    if (::qualityLevel != value) {
        ::qualityLevel.setParam(value);
        emit qualityLevelChanged(value);
    }
}

void ViewerConfig::setEncNone(bool value)
{
    if (m_encNone != value) {
        m_encNone = value;
        emit encNoneChanged(value);
    }
}

void ViewerConfig::setEncTLSAnon(bool value)
{
    if (m_encTLSAnon != value) {
        m_encTLSAnon = value;
        emit encTLSAnonChanged(value);
    }
}

void ViewerConfig::setEncTLSX509(bool value)
{
    if (m_encTLSX509 != value) {
        m_encTLSX509 = value;
        emit encTLSX509Changed(value);
    }
}

void ViewerConfig::setEncAES(bool value)
{
    if (m_encAES != value) {
        m_encAES = value;
        emit encAESChanged(value);
    }
}

void ViewerConfig::setAuthNone(bool value)
{
    if (m_authNone != value) {
        m_authNone = value;
        emit authNoneChanged(value);
    }
}

void ViewerConfig::setAuthVNC(bool value)
{
    if (m_authVNC != value) {
        m_authVNC = value;
        emit authVNCChanged(value);
    }
}

void ViewerConfig::setAuthPlain(bool value)
{
    if (m_authPlain != value) {
        m_authPlain = value;
        emit authPlainChanged(value);
    }
}

QString ViewerConfig::x509CA() const
{
#ifdef HAVE_GNUTLS
    return QString(rfb::CSecurityTLS::X509CA);
#else
    return QString();
#endif
}

void ViewerConfig::setX509CA(QString value)
{
#ifdef HAVE_GNUTLS
    if (rfb::CSecurityTLS::X509CA != value) {
        rfb::CSecurityTLS::X509CA.setParam(value.toStdString().c_str());
        emit x509CAChanged(value);
    }
#else
    Q_UNUSED(value)
#endif
}

QString ViewerConfig::x509CRL() const
{
#ifdef HAVE_GNUTLS
    return QString(rfb::CSecurityTLS::X509CRL);
#else
    return QString();
#endif
}

void ViewerConfig::setX509CRL(QString value)
{
#ifdef HAVE_GNUTLS
    if (rfb::CSecurityTLS::X509CRL != value) {
        rfb::CSecurityTLS::X509CRL.setParam(value.toStdString().c_str());
        emit x509CRLChanged(value);
    }
#else
    Q_UNUSED(value)
#endif
}

bool ViewerConfig::viewOnly() const
{
    return ::viewOnly;
}

void ViewerConfig::setViewOnly(bool value)
{
    if (::viewOnly != value) {
        ::viewOnly.setParam(value);
        emit viewOnlyChanged(value);
    }
}

bool ViewerConfig::emulateMiddleButton() const
{
    return ::emulateMiddleButton;
}

void ViewerConfig::setEmulateMiddleButton(bool value)
{
    if (::emulateMiddleButton != value) {
        ::emulateMiddleButton.setParam(value);
        emit emulateMiddleButtonChanged(value);
    }
}

bool ViewerConfig::dotWhenNoCursor() const
{
    return ::dotWhenNoCursor;
}

void ViewerConfig::setDotWhenNoCursor(bool value)
{
    if (::dotWhenNoCursor != value) {
        ::dotWhenNoCursor.setParam(value);
        emit dotWhenNoCursorChanged(value);
    }
}

bool ViewerConfig::fullscreenSystemKeys() const
{
    return ::fullscreenSystemKeys;
}

void ViewerConfig::setFullscreenSystemKeys(bool value)
{
    if (::fullscreenSystemKeys != value) {
        ::fullscreenSystemKeys.setParam(value);
        emit fullscreenSystemKeysChanged(value);
    }
}

QString ViewerConfig::menuKey() const
{
    return QString(::menuKey);
}

void ViewerConfig::setMenuKey(QString value)
{
    if (::menuKey != value) {
        ::menuKey.setParam(value.toStdString().c_str());
        emit menuKeyChanged(value);
        emit menuKeyIndexChanged(value);
    }
}

int ViewerConfig::menuKeyIndex() const
{
    for (int i = 0; i < m_menuKeys.size(); i++) {
        if (m_menuKeys[i] == ::menuKey) {
            return i;
        }
    }
    return -1;
}

bool ViewerConfig::acceptClipboard() const
{
    return ::acceptClipboard;
}

void ViewerConfig::setAcceptClipboard(bool value)
{
    if (::acceptClipboard != value) {
        ::acceptClipboard.setParam(value);
        emit acceptClipboardChanged(value);
    }
}

bool ViewerConfig::sendClipboard() const
{
    return ::sendClipboard;
}

void ViewerConfig::setSendClipboard(bool value)
{
    if (::sendClipboard != value) {
        ::sendClipboard.setParam(value);
        emit sendClipboardChanged(value);
    }
}

bool ViewerConfig::fullScreen() const
{
    return ::fullScreen;
}

void ViewerConfig::setFullScreen(bool value)
{
    if (::fullScreen != value) {
        ::fullScreen.setParam(value);
        emit fullScreenChanged(value);
    }
}

ViewerConfig::FullScreenMode ViewerConfig::fullScreenMode() const
{
    QString mode = QString(::fullScreenMode).toLower();
    return mode == "current" ? FSCurrent : mode == "all" ? FSAll : FSSelected;
}

void ViewerConfig::setFullScreenMode(ViewerConfig::FullScreenMode mode)
{
    QString value = mode == FSCurrent ? "Current" : mode == FSAll ? "All" : "Selected";
    if (::fullScreenMode != value) {
        ::fullScreenMode.setParam(value.toStdString().c_str());
        emit fullScreenModeChanged(mode);
    }
}

QList<int> ViewerConfig::selectedScreens() const
{
    QList<int> screens;
    std::set<int> monitors = ::fullScreenSelectedMonitors.getParam();
    for (int monitor : monitors) {
        screens.append(monitor);
    }
    return screens;
}

void ViewerConfig::setSelectedScreens(QList<int> value)
{
    std::set<int> screens;
    for (int screen : value) {
        screens.insert(screen);
    }
    ::fullScreenSelectedMonitors.setParam(screens);
}

bool ViewerConfig::shared() const
{
    return ::shared;
}

void ViewerConfig::setShared(bool value)
{
    if (::shared != value) {
        ::shared.setParam(value);
        emit sharedChanged(value);
    }
}

bool ViewerConfig::reconnectOnError() const
{
    return ::reconnectOnError;
}

void ViewerConfig::setReconnectOnError(bool value)
{
    if (::reconnectOnError != value) {
        ::reconnectOnError.setParam(value);
        emit reconnectOnErrorChanged(value);
    }
}

void ViewerConfig::handleOptions()
{
  // Checking all the details of the current set of encodings is just
  // a pain. Assume something has changed, as resending the encoding
  // list is cheap. Avoid overriding what the auto logic has selected
  // though.
  QVNCConnection *cc = AppManager::instance()->connection();
  if (!::autoSelect) {
    int encNum = encodingNum(::preferredEncoding);

    if (encNum != -1)
      cc->setPreferredEncoding(encNum);
  }

  if (::customCompressLevel)
    cc->setCompressLevel(::compressLevel);
  else
    cc->setCompressLevel(-1);

  if (!::noJpeg && !::autoSelect)
    cc->setQualityLevel(::qualityLevel);
  else
    cc->setQualityLevel(-1);

  cc->updatePixelFormat();
}
