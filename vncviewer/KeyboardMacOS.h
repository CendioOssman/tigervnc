#ifndef X11KEYBOARDHANDLER_H
#define X11KEYBOARDHANDLER_H

#include "Keyboard.h"

class NSView;
class NSCursor;

class KeyboardMacOS : public Keyboard
{
  Q_OBJECT

public:
  KeyboardMacOS(KeyboardHandler* handler, QObject* parent);

  bool handleEvent(const char* eventType, void* message) override;

  static bool isKeyboardSync(QByteArray const& eventType, void* message);

public slots:
  unsigned getLEDState() override;
  void setLEDState(unsigned state) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

signals:
  void message(QString const& msg, int timeout);

private:
  NSView* view;
  NSCursor* cursor;
};

#endif // VNCMACVIEW_H
