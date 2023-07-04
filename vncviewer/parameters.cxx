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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QApplication>
#include <QQmlEngine>
#include <QDir>
#include <QTextStream>

#if !defined(WIN32)
#include <sys/stat.h>
#else
#include <QThread>
#endif
#if defined(__APPLE__)
#include <Carbon/Carbon.h>
#include "cocoa.h"
#endif

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
#include "MonitorIndicesParameter.h"
#undef asprintf

#define SERVER_HISTORY_SIZE 20

using namespace rfb;
using namespace std;

static LogWriter vlog("Parameters");

namespace os {
  extern const char *getvnchomedir();
}
ViewerConfig *ViewerConfig::config_;

IntParameter pointerEventInterval("PointerEventInterval",
                                  "Time in milliseconds to rate-limit"
                                  " successive pointer events", 17);
BoolParameter emulateMiddleButton("EmulateMiddleButton",
                                  "Emulate middle mouse button by pressing "
                                  "left and right mouse buttons simultaneously",
                                  false);
BoolParameter dotWhenNoCursor("DotWhenNoCursor",
                              "Show the dot cursor when the server sends an "
                              "invisible cursor", false);

BoolParameter alertOnFatalError("AlertOnFatalError",
                                "Give a dialog on connection problems rather "
                                "than exiting immediately", true);

BoolParameter reconnectOnError("ReconnectOnError",
                               "Give a dialog on connection problems rather "
                               "than exiting immediately and ask for a reconnect.", true);

StringParameter passwordFile("PasswordFile",
                             "Password file for VNC authentication", "");
AliasParameter passwd("passwd", "Alias for PasswordFile", &passwordFile);

BoolParameter autoSelect("AutoSelect",
                         "Auto select pixel format and encoding. "
                         "Default if PreferredEncoding and FullColor are not specified.", 
                         true);
BoolParameter fullColour("FullColor",
                         "Use full color", true);
AliasParameter fullColourAlias("FullColour", "Alias for FullColor", &fullColour);
IntParameter lowColourLevel("LowColorLevel",
                            "Color level to use on slow connections. "
                            "0 = Very Low, 1 = Low, 2 = Medium", 2);
AliasParameter lowColourLevelAlias("LowColourLevel", "Alias for LowColorLevel", &lowColourLevel);
StringParameter preferredEncoding("PreferredEncoding",
                                  "Preferred encoding to use (Tight, ZRLE, Hextile or"
                                  " Raw)", "Tight");
BoolParameter customCompressLevel("CustomCompressLevel",
                                  "Use custom compression level. "
                                  "Default if CompressLevel is specified.", false);
IntParameter compressLevel("CompressLevel",
                           "Use specified compression level 0 = Low, 9 = High",
                           2);
BoolParameter noJpeg("NoJPEG",
                     "Disable lossy JPEG compression in Tight encoding.",
                     false);
IntParameter qualityLevel("QualityLevel",
                          "JPEG quality level. 0 = Low, 9 = High",
                          8);

BoolParameter maximize("Maximize", "Maximize viewer window", false);
BoolParameter fullScreen("FullScreen", "Enable full screen", false);
StringParameter fullScreenMode("FullScreenMode", "Specify which monitors to use when in full screen. "
                                                 "Should be either Current, Selected or All",
                                                 "Current");
BoolParameter fullScreenAllMonitors("FullScreenAllMonitors",
                                    "[DEPRECATED] Enable full screen over all monitors",
                                    false);
MonitorIndicesParameter fullScreenSelectedMonitors("FullScreenSelectedMonitors",
                                         "Use the given list of monitors in full screen"
                                         " when -FullScreenMode=Selected.",
                                         "1");
StringParameter desktopSize("DesktopSize",
                            "Reconfigure desktop size on the server on "
                            "connect (if possible)", "");
StringParameter geometry("geometry",
                         "Specify size and position of viewer window", "");

BoolParameter listenMode("listen", "Listen for connections from VNC servers", false);

BoolParameter remoteResize("RemoteResize",
                           "Dynamically resize the remote desktop size as "
                           "the size of the local client window changes. "
                           "(Does not work with all servers)", true);

BoolParameter viewOnly("ViewOnly",
                       "Don't send any mouse or keyboard events to the server",
                       false);
BoolParameter shared("Shared",
                     "Don't disconnect other viewers upon connection - "
                     "share the desktop instead",
                     false);

BoolParameter acceptClipboard("AcceptClipboard",
                              "Accept clipboard changes from the server",
                              true);
BoolParameter sendClipboard("SendClipboard",
                            "Send clipboard changes to the server", true);
#if !defined(WIN32) && !defined(__APPLE__)
BoolParameter setPrimary("SetPrimary",
                         "Set the primary selection as well as the "
                         "clipboard selection", true);
BoolParameter sendPrimary("SendPrimary",
                          "Send the primary selection to the "
                          "server as well as the clipboard selection",
                          true);
StringParameter display("display",
			"Specifies the X display on which the VNC viewer window should appear.",
			"");
#endif

StringParameter menuKey("MenuKey", "The key which brings up the popup menu",
                        "F8");

BoolParameter fullscreenSystemKeys("FullscreenSystemKeys",
                                   "Pass special keys (like Alt+Tab) directly "
                                   "to the server when in full-screen mode.",
                                   true);

StringParameter via("via", "Gateway to tunnel via", "");

static const char* IDENTIFIER_STRING = "TigerVNC Configuration file Version 1.0";

/*
 * We only save the sub set of parameters that can be modified from
 * the graphical user interface
 */
static VoidParameter* parameterArray[] = {
  /* Security */
#ifdef HAVE_GNUTLS
  &CSecurityTLS::X509CA,
  &CSecurityTLS::X509CRL,
#endif // HAVE_GNUTLS
  &SecurityClient::secTypes,
  /* Misc. */
  &reconnectOnError,
  &shared,
  /* Compression */
  &autoSelect,
  &fullColour,
  &lowColourLevel,
  &preferredEncoding,
  &customCompressLevel,
  &compressLevel,
  &noJpeg,
  &qualityLevel,
  /* Display */
  &fullScreen,
  &fullScreenMode,
  &fullScreenSelectedMonitors,
  /* Input */
  &viewOnly,
  &emulateMiddleButton,
  &dotWhenNoCursor,
  &acceptClipboard,
  &sendClipboard,
#if !defined(WIN32) && !defined(__APPLE__)
  &sendPrimary,
  &setPrimary,
#endif
  &menuKey,
  &fullscreenSystemKeys
};

static VoidParameter* readOnlyParameterArray[] = {
  &fullScreenAllMonitors
};

// Encoding Table
static const struct EscapeMap {
  const char first;
  const char second;
} replaceMap[] = { { '\n', 'n' },
                   { '\r', 'r' },
                   { '\\', '\\' } };

static bool encodeValue(const char* val, char* dest, size_t destSize) {

  size_t pos = 0;

  for (size_t i = 0; (val[i] != '\0') && (i < (destSize - 1)); i++) {
    bool normalCharacter;
    
    // Check for sequences which will need encoding
    normalCharacter = true;
    for (EscapeMap esc : replaceMap) {

      if (val[i] == esc.first) {
        dest[pos] = '\\';
        pos++;
        if (pos >= destSize)
          return false;

        dest[pos] = esc.second;
        normalCharacter = false;
        break;
      }

      if (normalCharacter) {
        dest[pos] = val[i];
      }
    }

    pos++;
    if (pos >= destSize)
      return false;
  }

  dest[pos] = '\0';
  return true;
}


static bool decodeValue(const char* val, char* dest, size_t destSize) {

  size_t pos = 0;
  
  for (size_t i = 0; (val[i] != '\0') && (i < (destSize - 1)); i++) {
    
    // Check for escape sequences
    if (val[i] == '\\') {
      bool escapedCharacter;
      
      escapedCharacter = false;
      for (EscapeMap esc : replaceMap) {
        if (val[i+1] == esc.second) {
          dest[pos] = esc.first;
          escapedCharacter = true;
          i++;
          break;
        }
      }

      if (!escapedCharacter)
        return false;

    } else {
      dest[pos] = val[i];
    }

    pos++;
    if (pos >= destSize) {
      return false;
    }
  }
  
  dest[pos] = '\0';
  return true;
}


#ifdef _WIN32
static void setKeyString(const char *_name, const char *_value, HKEY* hKey) {

  const DWORD buffersize = 256;

  wchar_t name[buffersize];
  unsigned size = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, _name, strlen(_name)+1, name, buffersize); // QT
  //unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw Exception(_("The name of the parameter is too large"));

  char encodingBuffer[buffersize];
  if (!encodeValue(_value, encodingBuffer, buffersize))
    throw Exception(_("The parameter is too large"));

  wchar_t value[buffersize];
  size = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, encodingBuffer, strlen(encodingBuffer)+1, value, buffersize); // QT
  //size = fl_utf8towc(encodingBuffer, strlen(encodingBuffer)+1, value, buffersize);
  if (size >= buffersize)
    throw Exception(_("The parameter is too large"));

  LONG res = RegSetValueExW(*hKey, name, 0, REG_SZ, (BYTE*)&value, (wcslen(value)+1)*2);
  if (res != ERROR_SUCCESS)
    throw rdr::SystemException("RegSetValueExW", res);
}


static void setKeyInt(const char *_name, const int _value, HKEY* hKey) {

  const DWORD buffersize = 256;
  wchar_t name[buffersize];
  DWORD value = _value;

  unsigned size = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, _name, strlen(_name)+1, name, buffersize); // QT
  //unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw Exception(_("The name of the parameter is too large"));

  LONG res = RegSetValueExW(*hKey, name, 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
  if (res != ERROR_SUCCESS)
    throw rdr::SystemException("RegSetValueExW", res);
}


static bool getKeyString(const char* _name, char* dest, size_t destSize, HKEY* hKey) {

  const DWORD buffersize = 256;
  wchar_t name[buffersize];
  WCHAR* value;
  DWORD valuesize;

  unsigned size = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, _name, strlen(_name)+1, name, buffersize); // QT
  //unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw Exception(_("The name of the parameter is too large"));

  value = new WCHAR[destSize];
  valuesize = destSize;
  LONG res = RegQueryValueExW(*hKey, name, nullptr, nullptr, (LPBYTE)value, &valuesize);
  if (res != ERROR_SUCCESS){
    delete [] value;
    if (res != ERROR_FILE_NOT_FOUND)
      throw rdr::SystemException("RegQueryValueExW", res);
    // The value does not exist, defaults will be used.
    return false;
  }

  char* utf8val = new char[destSize];
  size = WideCharToMultiByte(CP_ACP, 0, value, wcslen(value)+1, utf8val, destSize, nullptr, FALSE); // QT
  //size = fl_utf8fromwc(utf8val, destSize, value, wcslen(value)+1);
  delete [] value;
  if (size >= destSize) {
    delete [] utf8val;
    throw Exception(_("The parameter is too large"));
  }

  bool ret = decodeValue(utf8val, dest, destSize);
  delete [] utf8val;

  if (!ret)
    throw Exception(_("Invalid format or too large value"));

  return true;
}


static bool getKeyInt(const char* _name, int* dest, HKEY* hKey) {

  const DWORD buffersize = 256;
  DWORD dwordsize = sizeof(DWORD);
  DWORD value = 0;
  wchar_t name[buffersize];

  unsigned size = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, _name, strlen(_name)+1, name, buffersize); // QT
  //unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw Exception(_("The name of the parameter is too large"));

  LONG res = RegQueryValueExW(*hKey, name, nullptr, nullptr, (LPBYTE)&value, &dwordsize);
  if (res != ERROR_SUCCESS){
    if (res != ERROR_FILE_NOT_FOUND)
      throw rdr::SystemException("RegQueryValueExW", res);
    // The value does not exist, defaults will be used.
    return false;
  }

  *dest = (int)value;

  return true;
}

static void removeValue(const char* _name, HKEY* hKey) {
  const DWORD buffersize = 256;
  wchar_t name[buffersize];

  unsigned size = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, _name, strlen(_name)+1, name, buffersize); // QT
  //unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw Exception(_("The name of the parameter is too large"));

  LONG res = RegDeleteValueW(*hKey, name);
  if (res != ERROR_SUCCESS) {
    if (res != ERROR_FILE_NOT_FOUND)
      throw rdr::SystemException("RegDeleteValueW", res);
    // The value does not exist, no need to remove it.
    return;
  }
}

void saveHistoryToRegKey(const QStringList &serverHistory) {
  HKEY hKey;
  LONG res = RegCreateKeyExW(HKEY_CURRENT_USER,
                             L"Software\\TigerVNC\\vncviewer\\history", 0, nullptr,
                             REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
                             &hKey, nullptr);

  if (res != ERROR_SUCCESS)
    throw rdr::SystemException(_("Failed to create registry key"), res);

  int index = 0;
  assert(SERVER_HISTORY_SIZE < 100);
  char indexString[3];

  try {
    while(index < serverHistory.size() && index <= SERVER_HISTORY_SIZE) {
      snprintf(indexString, 3, "%d", (int)index);
      setKeyString(indexString, serverHistory[index].toUtf8(), &hKey);
      index++;
    }
  } catch (Exception& e) {
    RegCloseKey(hKey);
    throw;
  }

  res = RegCloseKey(hKey);
  if (res != ERROR_SUCCESS)
    throw rdr::SystemException(_("Failed to close registry key"), res);
}

static void saveToReg(const char* servername) {
  HKEY hKey;
    
  LONG res = RegCreateKeyExW(HKEY_CURRENT_USER,
                             L"Software\\TigerVNC\\vncviewer", 0, nullptr,
                             REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
                             &hKey, nullptr);
  if (res != ERROR_SUCCESS)
    throw rdr::SystemException(_("Failed to create registry key"), res);

  try {
    setKeyString("ServerName", servername, &hKey);
  } catch (Exception& e) {
    RegCloseKey(hKey);
    throw Exception(_("Failed to save \"%s\": %s"),
                    "ServerName", e.str());
  }

  for (size_t i = 0; i < sizeof(parameterArray)/sizeof(VoidParameter*); i++) {
    try {
      if (dynamic_cast<StringParameter*>(parameterArray[i]) != nullptr) {
        setKeyString(parameterArray[i]->getName(), *(StringParameter*)parameterArray[i], &hKey);
      } else if (dynamic_cast<IntParameter*>(parameterArray[i]) != nullptr) {
        setKeyInt(parameterArray[i]->getName(), (int)*(IntParameter*)parameterArray[i], &hKey);
      } else if (dynamic_cast<BoolParameter*>(parameterArray[i]) != nullptr) {
        setKeyInt(parameterArray[i]->getName(), (int)*(BoolParameter*)parameterArray[i], &hKey);
      } else {
        throw Exception(_("Unknown parameter type"));
      }
    } catch (Exception& e) {
      RegCloseKey(hKey);
      throw Exception(_("Failed to save \"%s\": %s"),
                      parameterArray[i]->getName(), e.str());
    }
  }

  // Remove read-only parameters to replicate the behaviour of Linux/macOS when they
  // store a config to disk. If the parameter hasn't been migrated at this point it
  // will be lost.
  for (size_t i = 0; i < sizeof(readOnlyParameterArray)/sizeof(VoidParameter*); i++) {
    try {
      removeValue(readOnlyParameterArray[i]->getName(), &hKey);
    } catch (Exception& e) {
      RegCloseKey(hKey);
      throw Exception(_("Failed to remove \"%s\": %s"),
                      readOnlyParameterArray[i]->getName(), e.str());
    }
  }

  res = RegCloseKey(hKey);
  if (res != ERROR_SUCCESS)
    throw rdr::SystemException(_("Failed to close registry key"), res);
}

void loadHistoryFromRegKey(QStringList& serverHistory) {
  HKEY hKey;
  list<string> serverHistory;

  LONG res = RegOpenKeyExW(HKEY_CURRENT_USER,
                           L"Software\\TigerVNC\\vncviewer\\history", 0,
                           KEY_READ, &hKey);
  if (res != ERROR_SUCCESS) {
    if (res == ERROR_FILE_NOT_FOUND) {
      // The key does not exist, defaults will be used.
      return serverHistory;
    }

    throw rdr::SystemException(_("Failed to open registry key"), res);
  }

  unsigned index;
  const DWORD buffersize = 256;
  char indexString[3];

  for (index = 0;;index++) {
    snprintf(indexString, 3, "%d", (int)index);
    char servernameBuffer[buffersize];

    try {
      if (!getKeyString(indexString, servernameBuffer,
                        buffersize, &hKey))
        break;
    } catch (Exception& e) {
      // Just ignore this entry and try the next one
      vlog.error(_("Failed to read server history entry %d: %s"),
                 (int)index, e.str());
      continue;
    }

    serverHistory.push_back(servernameBuffer);
  }

  res = RegCloseKey(hKey);
  if (res != ERROR_SUCCESS)
    throw rdr::SystemException(_("Failed to close registry key"), res);

  return serverHistory;
}

static void getParametersFromReg(VoidParameter* parameters[],
                                 size_t parameters_len, HKEY* hKey)
{
  const size_t buffersize = 256;
  int intValue = 0;
  char stringValue[buffersize];

  for (size_t i = 0; i < parameters_len/sizeof(VoidParameter*); i++) {
    try {
      if (dynamic_cast<StringParameter*>(parameters[i]) != nullptr) {
        if (getKeyString(parameters[i]->getName(), stringValue, buffersize, hKey))
          parameters[i]->setParam(stringValue);
      } else if (dynamic_cast<IntParameter*>(parameters[i]) != nullptr) {
        if (getKeyInt(parameters[i]->getName(), &intValue, hKey))
          ((IntParameter*)parameters[i])->setParam(intValue);
      } else if (dynamic_cast<BoolParameter*>(parameters[i]) != nullptr) {
        if (getKeyInt(parameters[i]->getName(), &intValue, hKey))
          ((BoolParameter*)parameters[i])->setParam(intValue);
      } else {
        throw Exception(_("Unknown parameter type"));
      }
    } catch(Exception& e) {
      // Just ignore this entry and continue with the rest
      vlog.error(_("Failed to read parameter \"%s\": %s"),
                 parameters[i]->getName(), e.str());
    }
  }
}

static char* loadFromReg() {
  HKEY hKey;

  LONG res = RegOpenKeyExW(HKEY_CURRENT_USER,
                           L"Software\\TigerVNC\\vncviewer", 0,
                           KEY_READ, &hKey);
  if (res != ERROR_SUCCESS) {
    if (res == ERROR_FILE_NOT_FOUND) {
      // The key does not exist, defaults will be used.
      return nullptr;
    }

    throw rdr::SystemException(_("Failed to open registry key"), res);
  }

  const size_t buffersize = 256;
  static char servername[buffersize];

  char servernameBuffer[buffersize];
  try {
    if (getKeyString("ServerName", servernameBuffer, buffersize, &hKey))
      snprintf(servername, buffersize, "%s", servernameBuffer);
  } catch(Exception& e) {
    vlog.error(_("Failed to read parameter \"%s\": %s"),
               "ServerName", e.str());
    strcpy(servername, "");
  }

  getParametersFromReg(parameterArray, sizeof(parameterArray), &hKey);
  getParametersFromReg(readOnlyParameterArray,
                       sizeof(readOnlyParameterArray), &hKey);

  res = RegCloseKey(hKey);
  if (res != ERROR_SUCCESS)
    throw rdr::SystemException(_("Failed to close registry key"), res);

  return servername;
}
#endif // _WIN32


void saveViewerParameters(const char *filename, const char *servername) {

  const size_t buffersize = 256;
  char filepath[PATH_MAX];
  char encodingBuffer[buffersize];

  // Write to the registry or a predefined file if no filename was specified.
  if(filename == nullptr) {

#ifdef _WIN32
    saveToReg(servername);
    return;
#else
    const char* configDir = os::getvncconfigdir();
    if (configDir == nullptr)
      throw Exception(_("Could not determine VNC config directory path"));

    snprintf(filepath, sizeof(filepath), "%s/default.tigervnc", configDir);
#endif
  } else {
    snprintf(filepath, sizeof(filepath), "%s", filename);
  }

  /* Write parameters to file */
  FILE* f = fopen(filepath, "w+");
  if (!f) {
    std::string msg = format(_("Could not open \"%s\""), filepath);
    throw rdr::SystemException(msg.c_str(), errno);
  }

  fprintf(f, "%s\n", IDENTIFIER_STRING);
  fprintf(f, "\n");

  if (!encodeValue(servername, encodingBuffer, buffersize)) {
    fclose(f);
    throw Exception(_("Failed to save \"%s\": %s"),
                    "ServerName", _("Could not encode parameter"));
  }
  fprintf(f, "ServerName=%s\n", encodingBuffer);

  for (VoidParameter* param : parameterArray) {
    if (dynamic_cast<StringParameter*>(param) != nullptr) {
      if (!encodeValue(*(StringParameter*)param,
          encodingBuffer, buffersize)) {
        fclose(f);
        throw Exception(_("Failed to save \"%s\": %s"),
                        param->getName(),
                        _("Could not encode parameter"));
      }
      fprintf(f, "%s=%s\n", ((StringParameter*)param)->getName(), encodingBuffer);
    } else if (dynamic_cast<IntParameter*>(param) != nullptr) {
      fprintf(f, "%s=%d\n", ((IntParameter*)param)->getName(), (int)*(IntParameter*)param);
    } else if (dynamic_cast<BoolParameter*>(param) != nullptr) {
      fprintf(f, "%s=%d\n", ((BoolParameter*)param)->getName(), (int)*(BoolParameter*)param);
    } else {      
      fclose(f);
      throw Exception(_("Failed to save \"%s\": %s"),
                      param->getName(),
                      _("Unknown parameter type"));
    }
  }
  fclose(f);
}

static bool findAndSetViewerParameterFromValue(
  VoidParameter* parameters[], size_t parameters_len,
  char* value, char* line)
{
  const size_t buffersize = 256;
  char decodingBuffer[buffersize];

  // Find and set the correct parameter
  for (size_t i = 0; i < parameters_len/sizeof(VoidParameter*); i++) {

    if (dynamic_cast<StringParameter*>(parameters[i]) != nullptr) {
      if (strcasecmp(line, ((StringParameter*)parameters[i])->getName()) == 0) {
        if(!decodeValue(value, decodingBuffer, sizeof(decodingBuffer)))
          throw Exception(_("Invalid format or too large value"));
        ((StringParameter*)parameters[i])->setParam(decodingBuffer);
        return false;
      }

    } else if (dynamic_cast<IntParameter*>(parameters[i]) != nullptr) {
      if (strcasecmp(line, ((IntParameter*)parameters[i])->getName()) == 0) {
        ((IntParameter*)parameters[i])->setParam(atoi(value));
        return false;
      }

    } else if (dynamic_cast<BoolParameter*>(parameters[i]) != nullptr) {
      if (strcasecmp(line, ((BoolParameter*)parameters[i])->getName()) == 0) {
        ((BoolParameter*)parameters[i])->setParam(atoi(value));
        return false;
      }

    } else {
      throw Exception(_("Unknown parameter type"));
    }
  }
  return true;
}

char* loadViewerParameters(const char *filename) {

  const size_t buffersize = 256;
  char filepath[PATH_MAX];
  char line[buffersize];
  char decodingBuffer[buffersize];
  static char servername[sizeof(line)];

  memset(servername, '\0', sizeof(servername));

  // Load from the registry or a predefined file if no filename was specified.
  if(filename == nullptr) {

#ifdef _WIN32
    return loadFromReg();
#else
    const char* configDir = os::getvncconfigdir();
    if (configDir == nullptr)
      throw Exception(_("Could not determine VNC config directory path"));

    snprintf(filepath, sizeof(filepath), "%s/default.tigervnc", configDir);
#endif
  } else {
    snprintf(filepath, sizeof(filepath), "%s", filename);
  }

  /* Read parameters from file */
  FILE* f = fopen(filepath, "r");
  if (!f) {
    if (!filename)
      return nullptr; // Use defaults.
    std::string msg = format(_("Could not open \"%s\""), filepath);
    throw rdr::SystemException(msg.c_str(), errno);
  }
  
  int lineNr = 0;
  while (!feof(f)) {

    // Read the next line
    lineNr++;
    if (!fgets(line, sizeof(line), f)) {
      if (feof(f))
        break;

      fclose(f);
      std::string msg = format(_("Failed to read line %d in "
                                 "file \"%s\""), lineNr, filepath);
      throw rdr::SystemException(msg.c_str(), errno);
    }

    if (strlen(line) == (sizeof(line) - 1)) {
      fclose(f);
      throw Exception(_("Failed to read line %d in file %s: %s"),
                      lineNr, filepath, _("Line too long"));
    }

    // Make sure that the first line of the file has the file identifier string
    if(lineNr == 1) {
      if(strncmp(line, IDENTIFIER_STRING, strlen(IDENTIFIER_STRING)) == 0)
        continue;

      fclose(f);
      throw Exception(_("Configuration file %s is in an invalid format"),
                      filepath);
    }
    
    // Skip empty lines and comments
    if ((line[0] == '\n') || (line[0] == '#') || (line[0] == '\r'))
      continue;

    int len = strlen(line);
    if (line[len-1] == '\n') {
      line[len-1] = '\0';
      len--;
    }
    if (line[len-1] == '\r') {
      line[len-1] = '\0';
      len--;
    }

    // Find the parameter value
    char *value = strchr(line, '=');
    if (value == nullptr) {
      vlog.error(_("Failed to read line %d in file %s: %s"),
                 lineNr, filepath, _("Invalid format"));
      continue;
    }
    *value = '\0'; // line only contains the parameter name below.
    value++;
    
    bool invalidParameterName = true; // Will be set to false below if 
                                      // the line contains a valid name.

    try {
      if (strcasecmp(line, "ServerName") == 0) {

        if(!decodeValue(value, decodingBuffer, sizeof(decodingBuffer)))
          throw Exception(_("Invalid format or too large value"));
        snprintf(servername, sizeof(decodingBuffer), "%s", decodingBuffer);
        invalidParameterName = false;

      } else {
        invalidParameterName = findAndSetViewerParameterFromValue(parameterArray, sizeof(parameterArray),
                                                                  value, line);

        if (invalidParameterName) {
          invalidParameterName = findAndSetViewerParameterFromValue(readOnlyParameterArray, sizeof(readOnlyParameterArray),
                                                                    value, line);
        }
      }
    } catch(Exception& e) {
      // Just ignore this entry and continue with the rest
      vlog.error(_("Failed to read line %d in file %s: %s"),
                 lineNr, filepath, e.str());
      continue;
    }

    if (invalidParameterName)
      vlog.error(_("Failed to read line %d in file %s: %s"),
                 lineNr, filepath, _("Unknown parameter"));
  }
  fclose(f);
  f = nullptr;

  return servername;
}

ViewerConfig::ViewerConfig()
 : QObject(nullptr)
 , encNone_(false)
 , encTLSAnon_(false)
 , encTLSX509_(false)
 , encAES_(false)
 , authNone_(false)
 , authVNC_(false)
 , authPlain_(false)
 , serverPort_(SERVER_PORT_OFFSET)
 , gatewayLocalPort_(0)
 , messageDir_(nullptr)
{
  connect(this, &ViewerConfig::accessPointChanged, this, [this](QString accessPoint) {
    serverHistory_.removeOne(accessPoint);
    serverHistory_.push_front(accessPoint);
    parseServerName();
    saveServerHistory();
    emit serverHistoryChanged(serverHistory_);
  }, Qt::QueuedConnection);
  initializeLogger();

  const char* homeDir = os::getvnchomedir();
  if (homeDir == nullptr) {
    QDir dir;
    if (!dir.mkpath(homeDir)) {
      vlog.error(_("Could not create VNC home directory:"));
    }
  }
  
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

    serverName_ = argv[i];
    i++;
  }
  // Check if the server name in reality is a configuration file
  potentiallyLoadConfigurationFile(serverName_);

  /* Specifying -via and -listen together is nonsense */
  if (::listenMode && ::via.getValueStr().length() > 0) {
    // TRANSLATORS: "Parameters" are command line arguments, or settings
    // from a file or the Windows registry.
    vlog.error(_("Parameters -listen and -via are incompatible"));
    QGuiApplication::exit(1);
  }

  loadServerHistory();
  parseServerName();

  rfb::Security security(rfb::SecurityClient::secTypes);
  auto secTypes = security.GetEnabledSecTypes();
  for (auto iter = secTypes.begin(); iter != secTypes.end(); ++iter) {
    switch (*iter) {
    case rfb::secTypeNone:
      encNone_ = true;
      authNone_ = true;
      break;
    case rfb::secTypeVncAuth:
      encNone_ = true;
      authVNC_ = true;
      break;
    }
  }
  auto secTypesExt = security.GetEnabledExtSecTypes();
  for (auto iterExt = secTypesExt.begin(); iterExt != secTypesExt.end(); ++iterExt) {
    switch (*iterExt) {
    case rfb::secTypePlain:
      encNone_ = true;
      authPlain_ = true;
      break;
    case rfb::secTypeTLSNone:
      encTLSAnon_ = true;
      authNone_ = true;
      break;
    case rfb::secTypeTLSVnc:
      encTLSAnon_ = true;
      authVNC_ = true;
      break;
    case rfb::secTypeTLSPlain:
      encTLSAnon_ = true;
      authPlain_ = true;
      break;
    case rfb::secTypeX509None:
      encTLSX509_ = true;
      authNone_ = true;
      break;
    case rfb::secTypeX509Vnc:
      encTLSX509_ = true;
      authVNC_ = true;
      break;
    case rfb::secTypeX509Plain:
      encTLSX509_ = true;
      authPlain_ = true;
      break;
    case rfb::secTypeRA2:
    case rfb::secTypeRA256:
      encAES_ = true;
      authVNC_ = true;
      authPlain_ = true;
      break;
    case rfb::secTypeRA2ne:
    case rfb::secTypeRAne256:
      authVNC_ = true;
      authPlain_ = true;
      break;
    }
  }
  auto keysyms = getMenuKeySymbols();
  for (int i = 0; i < getMenuKeySymbolCount(); i++) {
    menuKeys_.append(keysyms[i].name);
  }
}

ViewerConfig::~ViewerConfig()
{
  if (messageDir_) {
    free(messageDir_);
  }
}

int ViewerConfig::initialize()
{
  config_ = new ViewerConfig();
  qRegisterMetaType<ViewerConfig::FullScreenMode>("ViewerConfig::FullScreenMode");
  qmlRegisterSingletonType<ViewerConfig>("Qt.TigerVNC", 1, 0, "Config", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    return config_;
  });
  return 0;
}

bool ViewerConfig::installedSecurity(int type) const
{
  return availableSecurityTypes_.contains(type);
}

bool ViewerConfig::enabledSecurity(int type) const
{
  auto found = availableSecurityTypes_.find(type);
  return found != availableSecurityTypes_.end() && *found;
}

QString ViewerConfig::toLocalFile(const QUrl url) const
{
  return QDir::toNativeSeparators(url.toLocalFile());
}

void ViewerConfig::saveViewerParameters(QString path, QString serverName)
{
  serverHistory_.removeOne(serverName);
  serverHistory_.push_front(serverName);
  parseServerName();
  saveServerHistory();
  ::saveViewerParameters(path.isEmpty() ? nullptr : path.toStdString().c_str(), serverName.toStdString().c_str());
  emit serverHistoryChanged(serverHistory_);
}

QString ViewerConfig::loadViewerParameters(QString path)
{
  return QString(::loadViewerParameters(path.trimmed().length() > 0 ? path.toStdString().c_str() : nullptr));
}

void ViewerConfig::loadServerHistory()
{
  serverHistory_.clear();

#ifdef _WIN32
  loadHistoryFromRegKey(serverHistory_);
  return;
#endif

  const char* homeDir = os::getvnchomedir();
  if (homeDir == nullptr)
    throw rdr::Exception("%s", tr("Could not obtain the home directory path").toStdString().c_str());

  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", homeDir, SERVER_HISTORY);

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

    serverHistory_.push_back(line);
  }

  fclose(f);
}

void ViewerConfig::setServerHistory(QStringList history)
{
  if (serverHistory_ != history) {
    serverHistory_ = history;
    saveServerHistory();
    emit serverHistoryChanged(history);
  }
}

void ViewerConfig::saveServerHistory()
{
  serverName_ = serverHistory_.length() > 0 ? serverHistory_[0] : "";
  parseServerName();
#ifdef _WIN32
  saveHistoryToRegKey(serverHistory_);
#else
  const char* homeDir = os::getvnchomedir();
  if (homeDir == nullptr) {
    throw rdr::Exception("%s", _("Could not obtain the home directory path"));
  }
  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", homeDir, SERVER_HISTORY);

  /* Write server history to file */
  FILE *f = fopen(filepath, "w");
  if (!f) {
    throw rdr::Exception(_("Could not open \"%s\": %s"), filepath, strerror(errno));
  }
  QTextStream stream(f, QIODevice::WriteOnly | QIODevice::WriteOnly);

  // Save the last X elements to the config file.
  for(int i = 0; i < serverHistory_.size() && i <= SERVER_HISTORY_SIZE; i++) {
    stream << serverHistory_[i] << "\n";
  }
  stream.flush();
  fclose(f);
#endif
}

QString ViewerConfig::desktopSize() const
{
  return QString(::desktopSize);
}

void ViewerConfig::setDesktopSize(QString value)
{
  if (desktopSize() != value) {
    ::desktopSize.setParam(value.toStdString().c_str());
    emit desktopSizeChanged(value);
  }
}

bool ViewerConfig::remoteResize() const
{
  return ::remoteResize;
}

void ViewerConfig::setRemoteResize(bool value)
{
  if (remoteResize() != value) {
    ::remoteResize.setParam(value);
    emit remoteResizeChanged(value);
  }
}

QString ViewerConfig::geometry() const
{
  return QString(::geometry);
}

void ViewerConfig::setGeometry(QString value)
{
  if (geometry() != value) {
    ::geometry.setParam(value.toStdString().c_str());
    emit geometryChanged(value);
  }
}

bool ViewerConfig::autoSelect() const
{
  return ::autoSelect;
}

void ViewerConfig::setAutoSelect(bool value)
{
  if (autoSelect() != value) {
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
  if (fullColour() != value) {
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
  if (lowColourLevel() != value) {
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
  if (preferredEncoding() != value) {
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
  if (customCompressLevel() != value) {
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
  if (compressLevel() != value) {
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
  if (noJpeg() != value) {
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
  if (qualityLevel() != value) {
    ::qualityLevel.setParam(value);
    emit qualityLevelChanged(value);
  }
}

void ViewerConfig::setEncNone(bool value)
{
  if (encNone_ != value) {
    encNone_ = value;
    emit encNoneChanged(value);
  }
}

void ViewerConfig::setEncTLSAnon(bool value)
{
  if (encTLSAnon_ != value) {
    encTLSAnon_ = value;
    emit encTLSAnonChanged(value);
  }
}

void ViewerConfig::setEncTLSX509(bool value)
{
  if (encTLSX509_ != value) {
    encTLSX509_ = value;
    emit encTLSX509Changed(value);
  }
}

void ViewerConfig::setEncAES(bool value)
{
  if (encAES_ != value) {
    encAES_ = value;
    emit encAESChanged(value);
  }
}

void ViewerConfig::setAuthNone(bool value)
{
  if (authNone_ != value) {
    authNone_ = value;
    emit authNoneChanged(value);
  }
}

void ViewerConfig::setAuthVNC(bool value)
{
  if (authVNC_ != value) {
    authVNC_ = value;
    emit authVNCChanged(value);
  }
}

void ViewerConfig::setAuthPlain(bool value)
{
  if (authPlain_ != value) {
    authPlain_ = value;
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
  if (viewOnly() != value) {
    ::viewOnly.setParam(value);
    emit viewOnlyChanged(value);
  }
}

int ViewerConfig::pointerEventInterval() const
{
  return ::pointerEventInterval;
}

void ViewerConfig::setPointerEventInterval(int value)
{
  if (pointerEventInterval() != value) {
    ::pointerEventInterval.setParam(value);
    emit pointerEventIntervalChanged(value);
  }
}

bool ViewerConfig::emulateMiddleButton() const
{
  return ::emulateMiddleButton;
}

void ViewerConfig::setEmulateMiddleButton(bool value)
{
  if (emulateMiddleButton() != value) {
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
  if (dotWhenNoCursor() != value) {
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
  if (fullscreenSystemKeys() != value) {
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
  if (menuKey() != value) {
    ::menuKey.setParam(value.toStdString().c_str());
    emit menuKeyChanged(value);
    emit menuKeyIndexChanged(value);
  }
}

int ViewerConfig::menuKeyIndex() const
{
  for (int i = 0; i < menuKeys_.size(); i++) {
    if (menuKeys_[i] == ::menuKey) {
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
  if (acceptClipboard() != value) {
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
  if (sendClipboard() != value) {
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
  if (fullScreen() != value) {
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
  if (selectedScreens() != value) {
    std::set<int> screens;
    for (int &screen : value) {
      screens.insert(screen);
    }
    ::fullScreenSelectedMonitors.setParam(screens);
    emit selectedScreensChanged(value);
  }
}

bool ViewerConfig::shared() const
{
  return ::shared;
}

void ViewerConfig::setShared(bool value)
{
  if (shared() != value) {
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
  if (reconnectOnError() != value) {
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
      serverName_ = loadViewerParameters(vncServerName);
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
  Sleep(INFINITE);
#endif

  QGuiApplication::exit(1);
  exit(1);
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
    messageDir_ = strdup(locale.absoluteFilePath().toStdString().c_str());
    bindtextdomain(PACKAGE_NAME, messageDir_);
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

void ViewerConfig::parseServerName()
{
  if (!QString(::via).isEmpty() && !gatewayLocalPort_) {
    network::initSockets();
    gatewayLocalPort_ = network::findFreeTcpPort();
  }
  bool ok;
  int ix = serverName_.indexOf(':');
  if (ix >= 0) {
    int ix2 = serverName_.indexOf("::");
    if (ix2 < 0) {
      int port = SERVER_PORT_OFFSET + serverName_.mid(ix + 1).toInt(&ok, 10);
      if (ok) {
        serverPort_ = port;
      }
    }
    else {
      int port = serverName_.mid(ix + 2).toInt(&ok, 10);
      if (ok) {
        serverPort_ = port;
      }
    }
    serverHost_ = serverName_.left(ix);
  }
  else {
    int port = serverName_.toInt(&ok, 10);
    if (ok) {
      serverPort_ = port;
    }
    else {
      serverHost_ = serverName_;
    }
  }
}

void ViewerConfig::setAccessPoint(QString accessPoint)
{
  emit accessPointChanged(accessPoint);
}

bool ViewerConfig::canFullScreenOnMultiDisplays() const
{
#if defined(__APPLE__)
  return !cocoa_displays_have_separate_spaces();
#else
  return true;
#endif
}
