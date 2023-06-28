#if !defined( _VNCCREDENTIAL_H )
#define _VNCCREDENTIAL_H

#include <QObject>
#include "rfb/UserPasswdGetter.h"
#include "rfb/UserMsgBox.h"

class VNCCredential : public QObject, public rfb::UserPasswdGetter, public rfb::UserMsgBox
{
  Q_OBJECT

public:
  VNCCredential();
  virtual ~VNCCredential();

  // UserPasswdGetter callbacks

  void getUserPasswd(bool secure, std::string *user, std::string *password) override;

  // UserMsgBox callbacks

  bool showMsgBox(int flags, const char* title, const char* text) override;
};

#endif // _VNCCREDENTIAL_H
