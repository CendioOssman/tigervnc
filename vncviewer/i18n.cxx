/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <core/i18n.h>

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QString>
#include <QTranslator>

#include "i18n.h"

static bool load_catalog(const char* catalog, const QString& location)
{
  QTranslator* translator;

  translator = new QTranslator(QCoreApplication::instance());
  if (!translator->load(QLocale::system(), catalog, "", location))
    return false;

  QCoreApplication::instance()->installTranslator(translator);

  return true;
}

// This is based on how KDE loads Qt's translations
static void load_catalogs(const QString& location)
{
  // FIXME: KDE first loads English translation for some reason. See:
  // https://invent.kde.org/frameworks/ki18n/-/blob/master/src/i18n/main.cpp

  // First try to load the primary catalog
  if (load_catalog("qt_", location))
    return;

  // For some languages, that is just a meta catalog, which might be
  // missing. Try loading the individual catalogs instead.
  const char* catalogs[] = {
      "qtbase_",
      "qtmultimedia_",
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
      "qtscript_",
      "qtxmlpatterns_",
#endif
  };
  for (const char* catalog : catalogs)
    load_catalog(catalog, location);
}

void i18n_qt_init()
{
  load_catalogs(QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  // FIXME: Only for static builds? Only fallback?
  load_catalogs(":/i18n");
}
