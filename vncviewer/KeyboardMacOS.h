#ifndef X11KEYBOARDHANDLER_H
#define X11KEYBOARDHANDLER_H

#include "Keyboard.h"

class NSView;
class NSCursor;

class KeyboardMacOS : public Keyboard
{
public:
  KeyboardMacOS(KeyboardHandler* handler);

  bool handleEvent(const char* eventType, void* message) override;

  static bool isKeyboardSync(QByteArray const& eventType, void* message);

  unsigned getLEDState() override;
  void setLEDState(unsigned state) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

private:
  NSView* view;
  NSCursor* cursor;
};

#endif // VNCMACVIEW_H
