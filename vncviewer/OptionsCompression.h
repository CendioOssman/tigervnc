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

class Fl_Check_Button;
class Fl_Round_Button;
class Fl_Int_Input;

class OptionsCompression : public OptionsPage
{
public:
  OptionsCompression(int tx, int ty, int tw, int th);

  void loadOptions() override;
  void storeOptions() override;

protected:
  void handleAutoselect();
  void handleCompression();
  void handleJpeg();

protected:
  Fl_Check_Button *autoselectCheckbox;

  Fl_Group *encodingGroup;
  Fl_Round_Button *tightButton;
  Fl_Round_Button *zrleButton;
  Fl_Round_Button *hextileButton;
#ifdef HAVE_H264
  Fl_Round_Button *h264Button;
#endif
  Fl_Round_Button *rawButton;

  Fl_Group *colorlevelGroup;
  Fl_Round_Button *fullcolorCheckbox;
  Fl_Round_Button *mediumcolorCheckbox;
  Fl_Round_Button *lowcolorCheckbox;
  Fl_Round_Button *verylowcolorCheckbox;

  Fl_Check_Button *compressionCheckbox;
  Fl_Check_Button *jpegCheckbox;
  Fl_Int_Input *compressionInput;
  Fl_Int_Input *jpegInput;
};

#endif
