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

#include <locale.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

#include "i18n.h"

static const char* getlocaledir()
{
#if defined(WIN32)
  static char localebuf[PATH_MAX];
  char *slash;

  GetModuleFileName(nullptr, localebuf, sizeof(localebuf));

  slash = strrchr(localebuf, '\\');
  if (slash == nullptr)
    return nullptr;

  *slash = '\0';

  if ((strlen(localebuf) + strlen("\\locale")) >= sizeof(localebuf))
    return nullptr;

  strcat(localebuf, "\\locale");

  return localebuf;
#elif defined(__APPLE__)
  CFBundleRef bundle;
  CFURLRef localeurl;
  CFStringRef localestr;
  Boolean ret;

  static char localebuf[PATH_MAX];

  bundle = CFBundleGetMainBundle();
  if (bundle == nullptr)
    return nullptr;

  localeurl = CFBundleCopyResourceURL(bundle, CFSTR("locale"),
                                      nullptr, nullptr);
  if (localeurl == nullptr)
    return nullptr;

  localestr = CFURLCopyFileSystemPath(localeurl, kCFURLPOSIXPathStyle);

  CFRelease(localeurl);

  ret = CFStringGetCString(localestr, localebuf, sizeof(localebuf),
                           kCFStringEncodingUTF8);
  if (!ret)
    return nullptr;

  return localebuf;
#else
  return CMAKE_INSTALL_FULL_LOCALEDIR;
#endif
}

void i18n_init()
{
  const char *localedir;

  setlocale(LC_ALL, "");

  localedir = getlocaledir();
  if (localedir == nullptr)
    fprintf(stderr, "Failed to determine locale directory\n");
  else
    bindtextdomain(PACKAGE_NAME, localedir);
  textdomain(PACKAGE_NAME);

  // Set gettext codeset to what our GUI toolkit uses. Since we are
  // passing strings from strerror/gai_strerror to the GUI, these must
  // be in GUI codeset as well.
  bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
  bind_textdomain_codeset("libc", "UTF-8");
}