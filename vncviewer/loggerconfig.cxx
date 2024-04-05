#include "loggerconfig.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "i18n.h"
#include "viewerconfig.h"
#include "rfb/Logger_stdio.h"
#include "rfb/LogWriter.h"

#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#if defined(__APPLE__)
#include "cocoa.h"

#include <Carbon/Carbon.h>
#endif


LoggerConfig::LoggerConfig()
{
  setlocale(LC_ALL, "");
#if defined(WIN32) && ENABLE_NLS
  // Quick workaround for the defect of gettext on Windows. Read the discussion at
  // https://github.com/msys2/MINGW-packages/issues/4059 for details.
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
    // According to the linux document, trailing '/locale' of the message directory path must be removed for passing it
    // to bindtextdomain() but in reallity '/locale' must be given to make gettext() work properly.
    messageDir = strdup(locale.absoluteFilePath().toStdString().c_str());
#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE_NAME, messageDir);
#endif
  }
#ifdef ENABLE_NLS
  textdomain(PACKAGE_NAME);
#endif

  // Write about text to console, still using normal locale codeset
  QString about = ViewerConfig::aboutText();
  fprintf(stderr, "\n%s\n", about.toStdString().c_str());

#ifdef ENABLE_NLS
  // Set gettext codeset to what our GUI toolkit uses. Since we are
  // passing strings from strerror/gai_strerror to the GUI, these must
  // be in GUI codeset as well.
  bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
  bind_textdomain_codeset("libc", "UTF-8");
#endif

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
#ifdef QT_DEBUG
  rfb::LogWriter::setLogParams("*:stderr:100");
#else
  rfb::LogWriter::setLogParams("*:stderr:30");
#endif
}

LoggerConfig::~LoggerConfig()
{
  if (messageDir)
    free(messageDir);
}

QString LoggerConfig::getlocaledir()
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

  localeurl = CFBundleCopyResourceURL(bundle, CFSTR("locale"), NULL, NULL);
  if (localeurl == NULL)
    return QString();

  localestr = CFURLCopyFileSystemPath(localeurl, kCFURLPOSIXPathStyle);

  CFRelease(localeurl);

  ret = CFStringGetCString(localestr, localebuf, sizeof(localebuf), kCFStringEncodingUTF8);
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
