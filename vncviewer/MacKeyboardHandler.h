#ifndef X11KEYBOARDHANDLER_H
#define X11KEYBOARDHANDLER_H

#include "BaseKeyboardHandler.h"

class NSView;
class NSCursor;

class MacKeyboardHandler : public BaseKeyboardHandler
{
  Q_OBJECT

public:
  MacKeyboardHandler(QObject* parent = nullptr);

  bool nativeEventFilter(QByteArray const& eventType, void* message, long* result) override;

  bool handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;
  bool handleKeyRelease(int keyCode) override;

public slots:
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

signals:
  void message(QString const& msg, int timeout);

private:
  NSView* view_;
  NSCursor* cursor_;
};

#endif // VNCMACVIEW_H
