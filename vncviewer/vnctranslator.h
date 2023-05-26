#ifndef VNCTRANSLATOR_H
#define VNCTRANSLATOR_H

#include <QTranslator>
#include "gettext.h"

class VNCTranslator : public QTranslator
{
public:
  QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1) const override
  {
    Q_UNUSED(context)
    Q_UNUSED(disambiguation)
    Q_UNUSED(n)
    return QString::fromUtf8(gettext(sourceText));
  }
};

#endif // VNCTRANSLATOR_H
