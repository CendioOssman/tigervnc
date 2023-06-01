#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QQmlEngine>
#include <QDir>
#include <QTextStream>

#if !defined(WIN32)
#include <sys/stat.h>
#else
#include <winsock2.h>
#include <windows.h>
#endif
#if defined(__APPLE__)
#include <Carbon/Carbon.h>
#endif

#include "viewerconfig.h"
#include "parameters.h"
#include "menukey.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "rfb/encodings.h"
#include "rfb/Security.h"
#include "rfb/SecurityClient.h"
#include "rfb/Logger_stdio.h"
#include "rfb/LogWriter.h"
#include "rfb/Configuration.h"
#ifdef HAVE_GNUTLS
#include "rfb/CSecurityTLS.h"
#endif
#include "rfb/Exception.h"
#include "network/TcpSocket.h"
#include "i18n.h"
#undef asprintf


static rfb::LogWriter vlog("viewerconfig");
extern int getvnchomedir(char **dirp);
ViewerConfig *ViewerConfig::m_config;

ViewerConfig::ViewerConfig()
 : QObject(nullptr)
 , m_encNone(false)
 , m_encTLSAnon(false)
 , m_encTLSX509(false)
 , m_encAES(false)
 , m_authNone(false)
 , m_authVNC(false)
 , m_authPlain(false)
 , m_serverPort(SERVER_PORT_OFFSET)
 , m_gatewayLocalPort(0)
 , m_messageDir(nullptr)
{
  connect(this, &ViewerConfig::accessPointChanged, this, [this](QString accessPoint) {
    m_serverHistory.push_front(accessPoint);
    parserServerName();
    emit serverHistoryChanged(m_serverHistory);
  }, Qt::QueuedConnection);
  initializeLogger();

  char* homeDir = nullptr;
  if (getvnchomedir(&homeDir) == -1) {
    QDir dir;
    if (!dir.mkpath(homeDir)) {
      vlog.error(_("Could not create VNC home directory:"));
    }
  }
  delete[] homeDir;
  
  rfb::Configuration::enableViewerParams();
  loadViewerParameters("");
  if (::fullScreenAllMonitors) {
    vlog.info(_("FullScreenAllMonitors is deprecated, set FullScreenMode to 'all' instead"));
    ::fullScreenMode.setParam("all");
  }
  QStringList argv = QGuiApplication::arguments();
  int argc = argv.length();
  for (int i = 1; i < argc;) {
    /* We need to resolve an ambiguity for booleans */
    if (argv[i][0] == '-' && i+1 < argc) {
      QString name = argv[i].mid(1);
      rfb::VoidParameter *param = rfb::Configuration::getParam(name.toStdString().c_str());
      if ((param != NULL) &&
          (dynamic_cast<rfb::BoolParameter*>(param) != NULL)) {
        QString opt = argv[i+1];
        if ((opt.compare("0") == 0) ||
            (opt.compare("1") == 0) ||
            (opt.compare("true", Qt::CaseInsensitive) == 0) ||
            (opt.compare("false", Qt::CaseInsensitive) == 0) ||
            (opt.compare("yes", Qt::CaseInsensitive) == 0) ||
            (opt.compare("no", Qt::CaseInsensitive) == 0)) {
          param->setParam(opt.toStdString().c_str());
          i += 2;
          continue;
        }
      }
    }

    if (rfb::Configuration::setParam(argv[i].toStdString().c_str())) {
      i++;
      continue;
    }

    if (argv[i][0] == '-') {
      if (i+1 < argc) {
        if (rfb::Configuration::setParam(argv[i].mid(1).toStdString().c_str(), argv[i+1].toStdString().c_str())) {
          i += 2;
          continue;
        }
      }

      usage();
    }

    m_serverName = argv[i];
    i++;
  }
  // Check if the server name in reality is a configuration file
  potentiallyLoadConfigurationFile(m_serverName);

  /* Specifying -via and -listen together is nonsense */
  if (::listenMode && strlen(::via.getValueStr()) > 0) {
    // TRANSLATORS: "Parameters" are command line arguments, or settings
    // from a file or the Windows registry.
    vlog.error(_("Parameters -listen and -via are incompatible"));
    QGuiApplication::exit(1);
  }

  loadServerHistory();
  m_serverName = m_serverHistory.length() > 0 ? m_serverHistory[0] : "";
  parserServerName();

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
}

ViewerConfig::~ViewerConfig()
{
  if (m_messageDir) {
    free(m_messageDir);
  }
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
    throw rdr::Exception(_("Could not open \"%s\": %s"), filepath, strerror(errno));
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
      throw rdr::Exception(_("Failed to read line %d in file %s: %s"), lineNr, filepath, strerror(errno));
    }

    int len = strlen(line);

    if (len == (sizeof(line) - 1)) {
      fclose(f);
      throw rdr::Exception(_("Failed to read line %d in file %s: %s"), lineNr, filepath, _("Line too long"));
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
  m_serverName = m_serverHistory.length() > 0 ? m_serverHistory[0] : "";
  parserServerName();
#ifdef _WIN32
  saveHistoryToRegKey(m_serverHistory);
#else
  char* homeDir = nullptr;
  if (getvnchomedir(&homeDir) == -1) {
    throw rdr::Exception("%s", _("Could not obtain the home directory path"));
  }
  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s%s", homeDir, SERVER_HISTORY);
  delete[] homeDir;

  /* Write server history to file */
  FILE *f = fopen(filepath, "w");
  if (!f) {
    throw rdr::Exception(_("Could not open \"%s\": %s"), filepath, strerror(errno));
  }
  QTextStream stream(f, QIODevice::WriteOnly | QIODevice::WriteOnly);

  // Save the last X elements to the config file.
  for(int i = 0; i < m_serverHistory.size() && i <= SERVER_HISTORY_SIZE; i++) {
    stream << m_serverHistory[i] << "\n";
  }
  stream.flush();
  fclose(f);
#endif
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

bool ViewerConfig::potentiallyLoadConfigurationFile(QString vncServerName)
{
  bool hasPathSeparator = vncServerName.contains('/') || vncServerName.contains('\\');
  if (hasPathSeparator) {
#ifndef WIN32
    struct stat sb;

    // This might be a UNIX socket, we need to check
    if (stat(vncServerName.toStdString().c_str(), &sb) == -1) {
      // Some access problem; let loadViewerParameters() deal with it...
    }
    else {
      if ((sb.st_mode & S_IFMT) == S_IFSOCK) {
        return true;
      }
    }
#endif

    try {
      m_serverName = loadViewerParameters(vncServerName);
    }
    catch (rfb::Exception& e) {
      vlog.error("%s", e.str());
      return false;
    }
  }
  return true;
}

QString ViewerConfig::aboutText()
{
  return QString::asprintf(_("TigerVNC Viewer v%s\n"
                   "Built on: %s\n"
                   "Copyright (C) 1999-%d TigerVNC Team and many others (see README.rst)\n"
                   "See https://www.tigervnc.org for information on TigerVNC."), PACKAGE_VERSION, BUILD_TIMESTAMP, 2022);
}

void ViewerConfig::usage()
{
#if 0
#ifdef WIN32
  // If we don't have a console then we need to create one for output
  if (GetConsoleWindow() == NULL) {
    AllocConsole();
  }
#endif
#endif

  QString argv0 = QGuiApplication::arguments().at(0);
  std::string str = argv0.toStdString();
  const char *programName = str.c_str();

  fprintf(stderr,
          "\n"
          "usage: %s [parameters] [host][:displayNum]\n"
          "       %s [parameters] [host][::port]\n"
#ifndef WIN32
          "       %s [parameters] [unix socket]\n"
#endif
          "       %s [parameters] -listen [port]\n"
          "       %s [parameters] [.tigervnc file]\n",
          programName, programName,
#ifndef WIN32
          programName,
#endif
          programName, programName);

#if !defined(WIN32) && !defined(__APPLE__)
  fprintf(stderr,"\n"
          "Options:\n\n"
          "  -display Xdisplay  - Specifies the X display for the viewer window\n"
          "  -geometry geometry - Initial position of the main VNC viewer window. See the\n"
          "                       man page for details.\n");
#endif

  fprintf(stderr,"\n"
          "Parameters can be turned on with -<param> or off with -<param>=0\n"
          "Parameters which take a value can be specified as "
          "-<param> <value>\n"
          "Other valid forms are <param>=<value> -<param>=<value> "
          "--<param>=<value>\n"
          "Parameter names are case-insensitive.  The parameters are:\n\n");
  rfb::Configuration::listParams(79, 14);

#if defined(WIN32)
  // Just wait for the user to kill the console window
  QThread::currentThread()->wait();
#endif

  QGuiApplication::exit(1);
}

QString ViewerConfig::getlocaledir()
{

#if defined(WIN32)
  QFileInfo app(QCoreApplication::applicationFilePath());
  QString locale = QDir::toNativeSeparators(app.absoluteDir().path()) + QDir::separator() + "locale";
#if defined(QT_DEBUG)
  if (!QFileInfo::exists(locale)) {
    QFileInfo deploy(app.absoluteDir().path() + "/deploy/locale");
    if (deploy.exists()) {
      locale = QDir::toNativeSeparators(deploy.absoluteFilePath());
    }
  }
#endif
  return locale;
#elif defined(__APPLE__)
  CFBundleRef bundle;
  CFURLRef localeurl;
  CFStringRef localestr;
  Boolean ret;

  static char localebuf[PATH_MAX];

  bundle = CFBundleGetMainBundle();
  if (bundle == NULL)
    return QString();

  localeurl = CFBundleCopyResourceURL(bundle, CFSTR("locale"),
                                      NULL, NULL);
  if (localeurl == NULL)
    return QString();

  localestr = CFURLCopyFileSystemPath(localeurl, kCFURLPOSIXPathStyle);

  CFRelease(localeurl);

  ret = CFStringGetCString(localestr, localebuf, sizeof(localebuf),
                           kCFStringEncodingUTF8);
  if (!ret)
    return QString();

  return localebuf;
#else
  QString locale(CMAKE_INSTALL_FULL_LOCALEDIR);
#if defined(QT_DEBUG)
  if (!QFileInfo::exists(locale)) {
    QFileInfo app(QCoreApplication::applicationFilePath());
    QFileInfo deploy(app.absoluteDir().path() + "/deploy/locale");
    if (deploy.exists()) {
      locale = QDir::toNativeSeparators(deploy.absoluteFilePath());
    }
  }
#endif
  return locale;
#endif
}

void ViewerConfig::initializeLogger()
{
  setlocale(LC_ALL, "");
#if defined(WIN32) && ENABLE_NLS
  // Quick workaround for the defect of gettext on Windows. Read the discussion at https://github.com/msys2/MINGW-packages/issues/4059 for details.
  QString elang = QString(qgetenv("LANGUAGE")).trimmed();
  if (elang.length() == 0) {
    qputenv("LANGUAGE", "en:C");
  }
#endif

  QString localedir = getlocaledir();
  if (localedir.isEmpty())
    fprintf(stderr, "Failed to determine locale directory\n");
  else {
    QFileInfo locale(localedir);
    // According to the linux document, trailing '/locale' of the message directory path must be removed for passing it to bindtextdomain()
    // but in reallity '/locale' must be given to make gettext() work properly.
    m_messageDir = strdup(locale.absoluteFilePath().toStdString().c_str());
    bindtextdomain(PACKAGE_NAME, m_messageDir);
  }
  textdomain(PACKAGE_NAME);

  // Write about text to console, still using normal locale codeset
  QString about = aboutText();
  fprintf(stderr,"\n%s\n", about.toStdString().c_str());

  // Set gettext codeset to what our GUI toolkit uses. Since we are
  // passing strings from strerror/gai_strerror to the GUI, these must
  // be in GUI codeset as well.
  bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
  bind_textdomain_codeset("libc", "UTF-8");

  rfb::initStdIOLoggers();
#ifdef WIN32
  QString tmp = "C:\\temp";
  if (!QFileInfo::exists(tmp)) {
    tmp = QString(qgetenv("TMP"));
    if (!QFileInfo::exists(tmp)) {
      tmp = QString(qgetenv("TEMP"));
    }
  }
  QString log = tmp + "\\vncviewer.log";
  rfb::initFileLogger(log.toStdString().c_str());
#else
  rfb::initFileLogger("/tmp/vncviewer.log");
#endif
  rfb::LogWriter::setLogParams("*:stderr:30");
}

bool ViewerConfig::listenModeEnabled() const
{
  return ::listenMode;
}

QString ViewerConfig::gatewayHost() const
{
  return QString(::via);
}

void ViewerConfig::parserServerName()
{
  if (!QString(::via).isEmpty() && !m_gatewayLocalPort) {
    network::initSockets();
    m_gatewayLocalPort = network::findFreeTcpPort();
  }
  bool ok;
  int ix = m_serverName.indexOf(':');
  if (ix >= 0) {
    int ix2 = m_serverName.indexOf("::");
    if (ix2 < 0) {
      int port = SERVER_PORT_OFFSET + m_serverName.midRef(ix + 1).toInt(&ok, 10);
      if (ok) {
        m_serverPort = port;
      }
    }
    else {
      int port = m_serverName.midRef(ix + 2).toInt(&ok, 10);
      if (ok) {
        m_serverPort = port;
      }
    }
    m_serverHost = m_serverName.left(ix);
  }
  else {
    int port = m_serverName.toInt(&ok, 10);
    if (ok) {
      m_serverPort = port;
    }
  }
}

void ViewerConfig::setAccessPoint(QString accessPoint)
{
  emit accessPointChanged(accessPoint);
}
