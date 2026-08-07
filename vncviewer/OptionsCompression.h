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

#ifndef __OPTIONSCOMPRESSION_H__
#define __OPTIONSCOMPRESSION_H__

#include "OptionsPage.h"

class QCheckBox;
class QGroupBox;
class QRadioButton;
class QSpinBox;

class OptionsCompression : public OptionsPage
{
  Q_OBJECT

public:
  OptionsCompression(QWidget* parent=nullptr);

  void loadOptions() override;
  void storeOptions() override;

protected:
  void handleAutoselect();
  void handleCompression();
  void handleJpeg();

protected:
  QCheckBox* autoselectCheckbox;

  QGroupBox* encodingGroup;
  QRadioButton* tightButton;
  QRadioButton* zrleButton;
  QRadioButton* hextileButton;
#ifdef HAVE_H264
  QRadioButton* h264Button;
#endif
  QRadioButton* rawButton;

  QGroupBox* colorlevelGroup;
  QRadioButton* fullcolorCheckbox;
  QRadioButton* mediumcolorCheckbox;
  QRadioButton* lowcolorCheckbox;
  QRadioButton* verylowcolorCheckbox;

  QCheckBox* compressionCheckbox;
  QCheckBox* jpegCheckbox;
  QSpinBox* compressionInput;
  QSpinBox* jpegInput;
};

#endif
