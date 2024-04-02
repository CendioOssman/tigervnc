#ifndef __LOGGERCONFIG_H__
#define __LOGGERCONFIG_H__

#include <QString>

class LoggerConfig
{
public:
  LoggerConfig();
  ~LoggerConfig();

private:
  QString getlocaledir();
  char* messageDir = nullptr;
};

#endif
