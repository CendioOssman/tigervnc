#if !defined( _VNCCREDENTIAL_H )
#define _VNCCREDENTIAL_H

#include <QObject>

class UserDialog : public QObject
{
  Q_OBJECT

public:
  UserDialog();
  virtual ~UserDialog();

  // UserPasswdGetter callbacks

  void getUserPasswd(bool secure, std::string *user, std::string *password);

  // UserMsgBox callbacks

  bool showMsgBox(int flags, const char* title, const char* text);
};

#endif // _VNCCREDENTIAL_H
