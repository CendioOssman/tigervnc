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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Int_Input.H>

#include <core/i18n.h>

#include <rfb/CConnection.h>
#include <rfb/encodings.h>

#include "OptionsCompression.h"
#include "parameters.h"

#include "fltk/layout.h"

OptionsCompression::OptionsCompression(int tx, int ty, int tw, int th)
  : OptionsPage(tx, ty, tw, th, _("Compression"))
{
  int orig_tx, orig_ty;
  int col1_ty, col2_ty;
  int half_width, full_width;

  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  full_width = tw - OUTER_MARGIN * 2;
  half_width = (full_width - INNER_MARGIN) / 2;

  /* Checkboxes */

  autoselectCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                     CHECK_MIN_WIDTH,
                                                     CHECK_HEIGHT,
                                                     _("Auto select")));
  autoselectCheckbox->callback(
    [](Fl_Widget*, void* data) {
      ((OptionsCompression*)data)->handleAutoselect();
    },
    this);
  ty += CHECK_HEIGHT + INNER_MARGIN;

  jpegCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                              CHECK_MIN_WIDTH,
                                              CHECK_HEIGHT,
                                              _("Allow JPEG compression")));
  ty += CHECK_HEIGHT + INNER_MARGIN;

  /* Two columns */
  orig_tx = tx;
  orig_ty = ty;

  /* VNC encoding box */
  ty += GROUP_LABEL_OFFSET;
  encodingGroup = new Fl_Group(tx, ty, half_width, 0,
                                _("Preferred encoding"));
  encodingGroup->box(FL_FLAT_BOX);
  encodingGroup->labelfont(FL_BOLD);
  encodingGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    tightButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                               RADIO_MIN_WIDTH,
                                               RADIO_HEIGHT,
                                               "Tight"));
    tightButton->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    jpegButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                              RADIO_MIN_WIDTH,
                                              RADIO_HEIGHT,
                                              "JPEG"));
    jpegButton->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    zrleButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                              RADIO_MIN_WIDTH,
                                              RADIO_HEIGHT,
                                              "ZRLE"));
    zrleButton->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    hextileButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                 RADIO_MIN_WIDTH,
                                                 RADIO_HEIGHT,
                                                 "Hextile"));
    hextileButton->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

#ifdef HAVE_H264
    h264Button = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                             RADIO_MIN_WIDTH,
                                             RADIO_HEIGHT,
                                             "H.264"));
    h264Button->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;
#endif

    rawButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                             RADIO_MIN_WIDTH,
                                             RADIO_HEIGHT,
                                             "Raw"));
    rawButton->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;
  }

  ty -= TIGHT_MARGIN;

  encodingGroup->end();
  /* Needed for resize to work sanely */
  encodingGroup->resizable(nullptr);
  encodingGroup->size(encodingGroup->w(), ty - encodingGroup->y());
  col1_ty = ty;

  /* Second column */
  tx = orig_tx + half_width + INNER_MARGIN;
  ty = orig_ty;

  /* Color box */
  ty += GROUP_LABEL_OFFSET;
  colorlevelGroup = new Fl_Group(tx, ty, half_width, 0, _("Color level"));
  colorlevelGroup->labelfont(FL_BOLD);
  colorlevelGroup->box(FL_FLAT_BOX);
  colorlevelGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    fullcolorCheckbox = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                     RADIO_MIN_WIDTH,
                                                     RADIO_HEIGHT,
                                                     _("Full")));
    fullcolorCheckbox->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    mediumcolorCheckbox = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                       RADIO_MIN_WIDTH,
                                                       RADIO_HEIGHT,
                                                       _("Medium")));
    mediumcolorCheckbox->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    lowcolorCheckbox = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                    RADIO_MIN_WIDTH,
                                                    RADIO_HEIGHT,
                                                    _("Low")));
    lowcolorCheckbox->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    verylowcolorCheckbox = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                        RADIO_MIN_WIDTH,
                                                        RADIO_HEIGHT,
                                                        _("Very low")));
    verylowcolorCheckbox->type(FL_RADIO_BUTTON);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;
  }

  ty -= TIGHT_MARGIN;

  colorlevelGroup->end();
  /* Needed for resize to work sanely */
  colorlevelGroup->resizable(nullptr);
  colorlevelGroup->size(colorlevelGroup->w(),
                        ty - colorlevelGroup->y());
  col2_ty = ty;

  /* Back to normal */
  tx = orig_tx;
  ty = (col1_ty > col2_ty ? col1_ty : col2_ty) + INNER_MARGIN;

  /* Compression */
  compressionCheckbox = new Fl_Check_Button(LBLRIGHT_B(tx, ty,
                                                       CHECK_MIN_WIDTH,
                                                       CHECK_HEIGHT,
                                                       _("Custom compression level")));
  compressionCheckbox->labelfont(FL_BOLD);
  compressionCheckbox->callback(
    [](Fl_Widget*, void* data) {
      ((OptionsCompression*)data)->handleCompression();
    },
    this);
  ty += CHECK_HEIGHT;
  compressionGroup = new Fl_Group(tx, ty, full_width, 0);
  compressionGroup->box(FL_FLAT_BOX);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    compressionInput = new Fl_Int_Input(tx, ty,
                                        INPUT_HEIGHT, INPUT_HEIGHT,
                                        _("level (0=fast, 9=best)"));
    compressionInput->align(FL_ALIGN_RIGHT);
    ty += INPUT_HEIGHT + INNER_MARGIN;
  }

  ty -= INNER_MARGIN;

  compressionGroup->end();
  /* Needed for resize to work sanely */
  compressionGroup->resizable(nullptr);
  compressionGroup->size(compressionGroup->w(),
                         ty - compressionGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  /* Quality */
  ty += GROUP_LABEL_OFFSET;
  qualityGroup = new Fl_Group(tx, ty, full_width, 0, _("Quality level"));
  qualityGroup->labelfont(FL_BOLD);
  qualityGroup->box(FL_FLAT_BOX);
  qualityGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    jpegInput = new Fl_Int_Input(tx, ty,
                                 INPUT_HEIGHT, INPUT_HEIGHT,
                                 _("quality (0=poor, 9=best)"));
    jpegInput->align(FL_ALIGN_RIGHT);
    ty += INPUT_HEIGHT + INNER_MARGIN;
  }

  ty -= INNER_MARGIN;

  qualityGroup->end();
  /* Needed for resize to work sanely */
  qualityGroup->resizable(nullptr);
  qualityGroup->size(qualityGroup->w(),
                     ty - qualityGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  end();
}

void OptionsCompression::loadOptions()
{
  autoselectCheckbox->value(autoSelect);
  jpegCheckbox->value(!rfb::CConnection::noJpeg);

  if (preferredEncoding == "Tight")
    tightButton->setonly();
  else if (preferredEncoding == "JPEG")
    jpegButton->setonly();
  else if (preferredEncoding == "ZRLE")
    zrleButton->setonly();
  else if (preferredEncoding == "Hextile")
    hextileButton->setonly();
#ifdef HAVE_H264
  else if (preferredEncoding == "H.264")
    h264Button->setonly();
#endif
  else if (preferredEncoding == "Raw")
    rawButton->setonly();

  if (fullColour)
    fullcolorCheckbox->setonly();
  else {
    switch (lowColourLevel) {
    case 0:
      verylowcolorCheckbox->setonly();
      break;
    case 1:
      lowcolorCheckbox->setonly();
      break;
    case 2:
      mediumcolorCheckbox->setonly();
      break;
    }
  }

  char digit[2] = "0";

  compressionCheckbox->value(customCompressLevel);
  digit[0] = '0' + compressLevel;
  compressionInput->value(digit);
  digit[0] = '0' + qualityLevel;
  jpegInput->value(digit);

  handleAutoselect();
  handleCompression();
}

void OptionsCompression::storeOptions()
{
  autoSelect.setParam(autoselectCheckbox->value());
  rfb::CConnection::noJpeg.setParam(!jpegCheckbox->value());

  if (tightButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingTight));
  else if (jpegButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingJPEG));
  else if (zrleButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingZRLE));
  else if (hextileButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingHextile));
#ifdef HAVE_H264
  else if (h264Button->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingH264));
#endif
  else if (rawButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingRaw));

  fullColour.setParam(fullcolorCheckbox->value());
  if (verylowcolorCheckbox->value())
    lowColourLevel.setParam(0);
  else if (lowcolorCheckbox->value())
    lowColourLevel.setParam(1);
  else if (mediumcolorCheckbox->value())
    lowColourLevel.setParam(2);

  customCompressLevel.setParam(compressionCheckbox->value());
  compressLevel.setParam(atoi(compressionInput->value()));
  qualityLevel.setParam(atoi(jpegInput->value()));
}

void OptionsCompression::handleAutoselect()
{
  if (autoselectCheckbox->value()) {
    encodingGroup->deactivate();
    colorlevelGroup->deactivate();
    qualityGroup->deactivate();
  } else {
    encodingGroup->activate();
    colorlevelGroup->activate();
    qualityGroup->activate();
  }
}

void OptionsCompression::handleCompression()
{
  if (compressionCheckbox->value())
    compressionInput->activate();
  else
    compressionInput->deactivate();
}
