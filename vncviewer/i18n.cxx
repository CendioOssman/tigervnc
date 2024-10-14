/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2024 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QString>
#include <QTranslator>

#include "i18n.h"

static QString getlocaledir()
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
  if (bundle == nullptr)
    return QString();

  localeurl = CFBundleCopyResourceURL(bundle, CFSTR("locale"), nullptr, nullptr);
  if (localeurl == nullptr)
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

static bool loadCatalog(const QString &catalog, const QString &location)
{
  QTranslator* qtTranslator = new QTranslator(QCoreApplication::instance());
  if (!qtTranslator->load(QLocale::system(), catalog, QString(), location)) {
    return false;
  }
  QCoreApplication::instance()->installTranslator(qtTranslator);
  return true;
}

static void installQtTranslators()
{
  // FIXME: KDE first loads English translation for some reason. See:
  // https://invent.kde.org/frameworks/ki18n/-/blob/master/src/i18n/main.cpp
#ifdef Q_OS_LINUX
  QString location = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#else
  QString location = ":/i18n";
#endif
  if (loadCatalog(QStringLiteral("qt_"), location)) {
    return;
  }
  const auto catalogs = {
      QStringLiteral("qtbase_"),
      QStringLiteral("qtscript_"),
      QStringLiteral("qtmultimedia_"),
      QStringLiteral("qtxmlpatterns_"),
  };
  for (const auto &catalog : catalogs) {
    loadCatalog(catalog, location);
  }
}

void i18n_init()
{
  setlocale(LC_ALL, "");

  QString localedir = getlocaledir();
  if (localedir.isEmpty())
    fprintf(stderr, "Failed to determine locale directory\n");
  else {
    QFileInfo locale(localedir);
    // According to the linux document, trailing '/locale' of the message directory path must be removed for passing it
    // to bindtextdomain() but in reallity '/locale' must be given to make gettext() work properly.
    char* messageDir = strdup(locale.absoluteFilePath().toStdString().c_str());
#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE_NAME, messageDir);
#endif
  }
#ifdef ENABLE_NLS
  textdomain(PACKAGE_NAME);
#endif

#ifdef ENABLE_NLS
  // Set gettext codeset to what our GUI toolkit uses. Since we are
  // passing strings from strerror/gai_strerror to the GUI, these must
  // be in GUI codeset as well.
  bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
  bind_textdomain_codeset("libc", "UTF-8");
#endif

  installQtTranslators();
}