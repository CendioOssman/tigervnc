#ifndef INPUTTAB_H
#define INPUTTAB_H

#include "../optionsdialog.h"

#include <QWidget>

class QCheckBox;
class QComboBox;

class InputTab : public TabElement
{
  Q_OBJECT

public:
  InputTab(QWidget* parent = nullptr);

  void apply();
  void reset();

private:
  QCheckBox* inputViewOnly;
  QCheckBox* inputMouseEmulateMiddleButton;
  QCheckBox* inputMouseShowDot;
  QCheckBox* inputKeyboardPassSystemKeys;
  QComboBox* inputKeyboardMenuKeyCombo;
  QCheckBox* inputClipboardFromServer;
  QCheckBox* inputClipboardToServer;
};

#endif // INPUTTAB_H
