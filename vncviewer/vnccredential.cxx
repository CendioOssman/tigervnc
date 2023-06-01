#include <QEventLoop>
#include "rfb/Exception.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "vnccredential.h"

VNCCredential::VNCCredential()
 : QObject(nullptr)
{
}

VNCCredential::~VNCCredential()
{
}

void VNCCredential::getUserPasswd(bool secure, char** user, char** password)
{
  bool userNeeded = user != nullptr;
  bool passwordNeeded = password != nullptr;
  bool canceled = false;
  AppManager *manager = AppManager::instance();
  emit manager->credentialRequested(secure, userNeeded, passwordNeeded);
  QEventLoop loop;
  connect(AppManager::instance(), &AppManager::authenticateRequested, this, [&](QString userText, QString passwordText) {
    if (userNeeded) {
      delete *user;
      *user = new char[userText.length() + 1];
      strncpy(*user, userText.toStdString().c_str(), userText.length());
      (*user)[userText.length()] = 0;
    }
    if (passwordNeeded) {
      delete *password;
      *password = new char[passwordText.length() + 1];
      strncpy(*password, passwordText.toStdString().c_str(), passwordText.length());
      (*password)[passwordText.length()] = 0;
    }
    loop.quit();
  });
  connect(AppManager::instance(), &AppManager::cancelAuthRequested, this, [&]() {
    loop.quit();
    canceled = true;
  });
  loop.exec();
  if (canceled) {
    throw rfb::AuthCancelledException();
  }
}

bool VNCCredential::showMsgBox(int flags, const char* title, const char* text)
{
  int result = 0;
  AppManager *manager = AppManager::instance();
  emit manager->messageDialogRequested(flags, title, text);
  QEventLoop loop;
  connect(AppManager::instance(), &AppManager::messageResponded, this, [&](int response) {
    result = response;
    loop.quit();
  });
  loop.exec();
  return result;
}
