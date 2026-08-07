/* Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifndef __OPTIONSINPUT_H__
#define __OPTIONSINPUT_H__

#include "OptionsPage.h"

class QCheckBox;
class QComboBox;
class QGroupBox;

class OptionsInput : public OptionsPage
{
  Q_OBJECT

public:
  OptionsInput(QWidget* parent=nullptr);

  void loadOptions() override;
  void storeOptions() override;

protected:
  void handleClipboard();

protected:
  QGroupBox* mouseGroup;
  QGroupBox* keyboardGroup;
  QGroupBox* clipboardGroup;
  QCheckBox* viewOnlyCheckbox;
  QCheckBox* emulateMBCheckbox;
  QCheckBox* dotCursorCheckbox;
  QCheckBox* systemKeysCheckbox;
  QComboBox* menuKeyChoice;
  QCheckBox* acceptClipboardCheckbox;
#if !defined(WIN32) && !defined(__APPLE__)
  QCheckBox* setPrimaryCheckbox;
#endif
  QCheckBox* sendClipboardCheckbox;
#if !defined(WIN32) && !defined(__APPLE__)
  QCheckBox* sendPrimaryCheckbox;
#endif
};

#endif
