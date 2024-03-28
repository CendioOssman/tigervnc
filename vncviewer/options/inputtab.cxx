#include "inputtab.h"

#include "parameters.h"

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
  model->setStringList(ViewerConfig::config()->getMenuKeys());
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
  inputClipboardToServer = new QCheckBox(tr("Send clipboard to server"));
  vbox3->addWidget(inputClipboardToServer);
  groupBox3->setLayout(vbox3);
  layout->addWidget(groupBox3);

  layout->addStretch(1);
  setLayout(layout);
}

void InputTab::apply()
{
  ViewerConfig::config()->setViewOnly(inputViewOnly->isChecked());
  ViewerConfig::config()->setEmulateMiddleButton(inputMouseEmulateMiddleButton->isChecked());
  ViewerConfig::config()->setDotWhenNoCursor(inputMouseShowDot->isChecked());
  ViewerConfig::config()->setFullscreenSystemKeys(inputKeyboardPassSystemKeys->isChecked());
  ViewerConfig::config()->setMenuKey(inputKeyboardMenuKeyCombo->currentText());
  ViewerConfig::config()->setAcceptClipboard(inputClipboardFromServer->isChecked());
  ViewerConfig::config()->setSendClipboard(inputClipboardToServer->isChecked());
}

void InputTab::reset()
{
  inputViewOnly->setChecked(ViewerConfig::config()->viewOnly());
  inputMouseEmulateMiddleButton->setChecked(ViewerConfig::config()->emulateMiddleButton());
  inputMouseShowDot->setChecked(ViewerConfig::config()->dotWhenNoCursor());
  inputKeyboardPassSystemKeys->setChecked(ViewerConfig::config()->fullscreenSystemKeys());
  inputKeyboardMenuKeyCombo->setCurrentIndex(ViewerConfig::config()->menuKeyIndex());
  inputClipboardFromServer->setChecked(ViewerConfig::config()->acceptClipboard());
  inputClipboardToServer->setChecked(ViewerConfig::config()->sendClipboard());
}
