#if !defined( _VNCCREDENTIAL_H )
#define _VNCCREDENTIAL_H

#include <QObject>

class VNCCredential : public QObject
{
  Q_OBJECT

public:
  VNCCredential();
  virtual ~VNCCredential();

  // UserPasswdGetter callbacks

  void getUserPasswd(bool secure, std::string *user, std::string *password);

  // UserMsgBox callbacks

  bool showMsgBox(int flags, const char* title, const char* text);
};

#endif // _VNCCREDENTIAL_H
