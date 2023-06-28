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

void VNCCredential::getUserPasswd(bool secure, std::string *user, std::string *password)
{
  bool userNeeded = user != nullptr;
  bool passwordNeeded = password != nullptr;
  bool canceled = false;
  AppManager *manager = AppManager::instance();
  QString envUsername = QString(qgetenv("VNC_USERNAME"));
  QString envPassword = QString(qgetenv("VNC_PASSWORD"));
  if (user && password && !envUsername.isEmpty() && !envPassword.isEmpty()) {
    user->assign(envUsername.toStdString());
    password->assign(envPassword.toStdString());
    return;
  }
  if (password && !envPassword.isEmpty()) {
    password->assign(envPassword.toStdString());
    return;
  }
  emit manager->credentialRequested(secure, userNeeded, passwordNeeded);
  QEventLoop loop;
  connect(AppManager::instance(), &AppManager::authenticateRequested, this, [&](QString userText, QString passwordText) {
    if (userNeeded) {
      user->assign(userText.toStdString());
    }
    if (passwordNeeded) {
      password->assign(passwordText.toStdString());
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
