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

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  bool nativeEventFilter(QByteArray const& eventType, void* message, long* result) override;
#else
  bool nativeEventFilter(QByteArray const& eventType, void* message, qintptr* result) override;
#endif
  bool handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;

public slots:
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

signals:
  void message(QString const& msg, int timeout);

private:
  NSView* view;
  NSCursor* cursor;
};

#endif // VNCMACVIEW_H
