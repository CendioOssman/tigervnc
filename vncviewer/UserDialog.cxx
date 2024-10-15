#include "UserDialog.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rfb/CConnection.h"
#include "rfb/Exception.h"
#include "rfb/obfuscate.h"
#include "appmanager.h"
#include "authdialog.h"
#include "i18n.h"
#include "parameters.h"

#include <QEventLoop>
#include <QMessageBox>

UserDialog::UserDialog()
 : QObject(nullptr)
{
}

UserDialog::~UserDialog()
{
}

void UserDialog::getUserPasswd(bool secure, std::string *user, std::string *password)
{
  const char *passwordFileName(::passwordFile);
  bool userNeeded = user != nullptr;
  bool passwordNeeded = password != nullptr;
  QString envUsername = QString(qgetenv("VNC_USERNAME"));
  QString envPassword = QString(qgetenv("VNC_PASSWORD"));
  if (user && !envUsername.isEmpty() && !envPassword.isEmpty()) {
    user->assign(envUsername.toStdString());
    password->assign(envPassword.toStdString());
    return;
  }
  if (!user && !envPassword.isEmpty()) {
    password->assign(envPassword.toStdString());
    return;
  }
  if (!user && passwordFileName[0]) {
    std::vector<uint8_t> obfPwd(256);
    FILE* fp;

    fp = fopen(passwordFileName, "rb");
    if (!fp)
      throw rfb::Exception(_("Opening password file failed"));

    obfPwd.resize(fread(obfPwd.data(), 1, obfPwd.size(), fp));
    fclose(fp);

    password->assign(rfb::deobfuscate(obfPwd.data(), obfPwd.size()));
    return;
  }

  AuthDialog d(secure, userNeeded, passwordNeeded);
  d.exec();
  if (d.result() != QDialog::Accepted) {
    throw rfb::Exception(_("Authentication cancelled"));
  }

  if (userNeeded) {
    user->assign(d.getUser().toStdString());
  }
  if (passwordNeeded) {
    password->assign(d.getPassword().toStdString());
  }
}

bool UserDialog::showMsgBox(int flags, const char* title, const char* text)
{
  QMessageBox* dlg;
  QMessageBox::StandardButtons buttons;
  QMessageBox::Icon icon;

  switch (flags & 0xf) {
  case rfb::M_OKCANCEL:
    buttons = QMessageBox::Ok | QMessageBox::Cancel;
    break;
  case rfb::M_YESNO:
    buttons = QMessageBox::Yes | QMessageBox::No;
    break;
  case rfb::M_OK:
    buttons = QMessageBox::Ok;
    break;
  default:
    buttons = QMessageBox::Close;
  }

  switch (flags & 0xf0) {
  case rfb::M_ICONERROR:
    icon = QMessageBox::Critical;
    break;
  case rfb::M_ICONWARNING:
    icon = QMessageBox::Warning;
    break;
  default:
    icon = QMessageBox::Information;
  }

  dlg = new QMessageBox(icon, title, text, buttons);
  AppManager::instance()->openDialog(dlg);

  return (dlg->result() == QMessageBox::Ok) ||
         (dlg->result() == QMessageBox::Yes);
}
