#include "inputtab.h"

#include "parameters.h"
#include "menukey.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QStringListModel>
#include <QVBoxLayout>

InputTab::InputTab(QWidget* parent)
  : TabElement{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  inputViewOnly = new QCheckBox(tr("View only (ignore mouse and keyboard)"));
  layout->addWidget(inputViewOnly);

  QGroupBox* groupBox1 = new QGroupBox(tr("Mouse"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  inputMouseEmulateMiddleButton = new QCheckBox(tr("Emulate middle mouse button"));
  vbox1->addWidget(inputMouseEmulateMiddleButton);
  inputMouseShowDot = new QCheckBox(tr("Show dot when no cursor"));
  vbox1->addWidget(inputMouseShowDot);
  groupBox1->setLayout(vbox1);
  layout->addWidget(groupBox1);

  QGroupBox* groupBox2 = new QGroupBox(tr("Keyboard"));
  QVBoxLayout* vbox2 = new QVBoxLayout;
  inputKeyboardPassSystemKeys = new QCheckBox(tr("Pass system keys directly to server (full screen)"));
  vbox2->addWidget(inputKeyboardPassSystemKeys);
  QHBoxLayout* hbox2 = new QHBoxLayout;
  QLabel* label = new QLabel(tr("Menu key"));
  hbox2->addWidget(label);
  inputKeyboardMenuKeyCombo = new QComboBox;
  QStringListModel* model = new QStringListModel;
  QStringList menuKeys;
  auto keysyms = getMenuKeySymbols();
  for (int i = 0; i < getMenuKeySymbolCount(); i++) {
    menuKeys.append(keysyms[i].name);
  }
  model->setStringList(menuKeys);
  inputKeyboardMenuKeyCombo->setModel(model);
  hbox2->addWidget(inputKeyboardMenuKeyCombo);
  hbox2->addStretch(1);
  vbox2->addLayout(hbox2);
  groupBox2->setLayout(vbox2);
  layout->addWidget(groupBox2);

  QGroupBox* groupBox3 = new QGroupBox(tr("Clipboard"));
  QVBoxLayout* vbox3 = new QVBoxLayout;
  inputClipboardFromServer = new QCheckBox(tr("Accept clipboard from server"));
  vbox3->addWidget(inputClipboardFromServer);
#if !defined(WIN32) && !defined(__APPLE__)
  inputSetPrimary = new QCheckBox(tr("Also set primary selection"));
  vbox3->addWidget(inputSetPrimary);
#endif
  inputClipboardToServer = new QCheckBox(tr("Send clipboard to server"));
  vbox3->addWidget(inputClipboardToServer);
#if !defined(WIN32) && !defined(__APPLE__)
  inputSendPrimary = new QCheckBox(tr("Send primary selection as keyboard"));
  vbox3->addWidget(inputSendPrimary);
#endif
  groupBox3->setLayout(vbox3);
  layout->addWidget(groupBox3);

  layout->addStretch(1);
  setLayout(layout);
}

void InputTab::apply()
{
  ::viewOnly.setParam(inputViewOnly->isChecked());
  ::emulateMiddleButton.setParam(inputMouseEmulateMiddleButton->isChecked());
  ::dotWhenNoCursor.setParam(inputMouseShowDot->isChecked());
  ::fullscreenSystemKeys.setParam(inputKeyboardPassSystemKeys->isChecked());
  ::menuKey.setParam(inputKeyboardMenuKeyCombo->currentText().toStdString().c_str());
  ::acceptClipboard.setParam(inputClipboardFromServer->isChecked());
  ::sendClipboard.setParam(inputClipboardToServer->isChecked());
#if !defined(WIN32) && !defined(__APPLE__)
  ::setPrimary.setParam(inputSetPrimary->isChecked());
  ::sendPrimary.setParam(inputSendPrimary->isChecked());
#endif
}

void InputTab::reset()
{
  inputViewOnly->setChecked(::viewOnly);
  inputMouseEmulateMiddleButton->setChecked(::emulateMiddleButton);
  inputMouseShowDot->setChecked(::dotWhenNoCursor);
  inputKeyboardPassSystemKeys->setChecked(::fullscreenSystemKeys);
  inputKeyboardMenuKeyCombo->setCurrentText(::menuKey.getValueStr().c_str());
  inputClipboardFromServer->setChecked(::acceptClipboard);
  inputClipboardToServer->setChecked(::sendClipboard);
#if !defined(WIN32) && !defined(__APPLE__)
  inputSetPrimary->setChecked(::setPrimary);
  inputSendPrimary->setChecked(::sendPrimary);
#endif
}
