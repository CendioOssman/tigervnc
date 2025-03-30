/* Copyright 2011-2021 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <assert.h>
#include <stdlib.h>
#include <list>

#include <rfb/encodings.h>

#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
#include <rfb/Security.h>
#include <rfb/SecurityClient.h>
#ifdef HAVE_GNUTLS
#include <rfb/CSecurityTLS.h>
#endif
#endif

#include "OptionsDialog.h"
#include "i18n.h"
#include "menukey.h"
#include "parameters.h"

#include "fltk/layout.h"
#include "fltk/util.h"
#include "fltk/Fl_Monitor_Arrangement.h"
#include "fltk/Fl_Navigation.h"

#include <FL/Fl.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Choice.H>

static std::set<OptionsDialog *> instances;

OptionsDialog::OptionsDialog()
  : Fl_Window(580, 420, _("TigerVNC options"))
{
  Fl_Navigation *navigation;

  // Odd dimensions to get rid of extra borders
  // FIXME: We need to retain the top border on Windows as they don't
  //        have any separator for the caption, which looks odd
#ifdef WIN32
  navigation = new Fl_Navigation(-1, 0, w()+2, h()+1);
#else
  navigation = new Fl_Navigation(-1, -1, w()+2, h()+2);
#endif
  {
    int tx, ty, tw, th;

    navigation->client_area(tx, ty, tw, th, 150);

    createCompressionPage(tx, ty, tw, th);
    createSecurityPage(tx, ty, tw, th);
    createInputPage(tx, ty, tw, th);
    createDisplayPage(tx, ty, tw, th);
    createMiscPage(tx, ty, tw, th);
  }

  navigation->end();

  set_modal();

  if (instances.size() == 0)
    Fl::add_handler(fltk_event_handler);
  instances.insert(this);
}


OptionsDialog::~OptionsDialog()
{
  instances.erase(this);

  if (instances.size() == 0)
    Fl::remove_handler(fltk_event_handler);
}


void OptionsDialog::showDialog(void)
{
  static OptionsDialog *dialog = nullptr;

  if (!dialog)
    dialog = new OptionsDialog();

  if (dialog->shown())
    return;

  dialog->show();
}


void OptionsDialog::show(void)
{
  /* show() gets called for raise events as well */
  if (!shown())
    loadOptions();

  Fl_Window::show();
}


void OptionsDialog::loadOptions(void)
{
  /* Compression */
  autoselectCheckbox->value(autoSelect);

  if (preferredEncoding == "Tight")
    tightButton->setonly();
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
  jpegCheckbox->value(!noJpeg);
  digit[0] = '0' + compressLevel;
  compressionInput->value(digit);
  digit[0] = '0' + qualityLevel;
  jpegInput->value(digit);

  handleAutoselect(autoselectCheckbox, this);
  handleCompression(compressionCheckbox, this);
  handleJpeg(jpegCheckbox, this);

#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
  /* Security */
  rfb::Security security(rfb::SecurityClient::secTypes);

  std::list<uint8_t> secTypes;

  std::list<uint32_t> secTypesExt;

  encNoneCheckbox->value(false);
#ifdef HAVE_GNUTLS
  encTLSCheckbox->value(false);
  encX509Checkbox->value(false);
#endif
#ifdef HAVE_NETTLE
  encRSAAESCheckbox->value(false);
#endif

  authNoneCheckbox->value(false);
  authVncCheckbox->value(false);
  authPlainCheckbox->value(false);

  secTypes = security.GetEnabledSecTypes();
  for (uint8_t type : secTypes) {
    switch (type) {
    case rfb::secTypeNone:
      encNoneCheckbox->value(true);
      authNoneCheckbox->value(true);
      break;
    case rfb::secTypeVncAuth:
      encNoneCheckbox->value(true);
      authVncCheckbox->value(true);
      break;
    }
  }

  secTypesExt = security.GetEnabledExtSecTypes();
  for (uint32_t type : secTypesExt) {
    switch (type) {
    case rfb::secTypePlain:
      encNoneCheckbox->value(true);
      authPlainCheckbox->value(true);
      break;
#ifdef HAVE_GNUTLS
    case rfb::secTypeTLSNone:
      encTLSCheckbox->value(true);
      authNoneCheckbox->value(true);
      break;
    case rfb::secTypeTLSVnc:
      encTLSCheckbox->value(true);
      authVncCheckbox->value(true);
      break;
    case rfb::secTypeTLSPlain:
      encTLSCheckbox->value(true);
      authPlainCheckbox->value(true);
      break;
    case rfb::secTypeX509None:
      encX509Checkbox->value(true);
      authNoneCheckbox->value(true);
      break;
    case rfb::secTypeX509Vnc:
      encX509Checkbox->value(true);
      authVncCheckbox->value(true);
      break;
    case rfb::secTypeX509Plain:
      encX509Checkbox->value(true);
      authPlainCheckbox->value(true);
      break;
#endif
#ifdef HAVE_NETTLE
    case rfb::secTypeRA2:
    case rfb::secTypeRA256:
      encRSAAESCheckbox->value(true);
      authVncCheckbox->value(true);
      authPlainCheckbox->value(true);
      break;
    case rfb::secTypeRA2ne:
    case rfb::secTypeRAne256:
      authVncCheckbox->value(true);
      /* fall through */
    case rfb::secTypeDH:
    case rfb::secTypeMSLogonII:
      encNoneCheckbox->value(true);
      authPlainCheckbox->value(true);
      break;
#endif
    
    }
  }

#ifdef HAVE_GNUTLS
  caInput->value(rfb::CSecurityTLS::X509CA);
  crlInput->value(rfb::CSecurityTLS::X509CRL);

  handleX509(encX509Checkbox, this);
#endif
#endif

  /* Input */
  viewOnlyCheckbox->value(viewOnly);
  emulateMBCheckbox->value(emulateMiddleButton);
  acceptClipboardCheckbox->value(acceptClipboard);
#if !defined(WIN32) && !defined(__APPLE__)
  setPrimaryCheckbox->value(setPrimary);
#endif
  sendClipboardCheckbox->value(sendClipboard);
#if !defined(WIN32) && !defined(__APPLE__)
  sendPrimaryCheckbox->value(sendPrimary);
#endif
  systemKeysCheckbox->value(fullscreenSystemKeys);

  menuKeyChoice->value(0);

  for (int idx = 0; idx < getMenuKeySymbolCount(); idx++)
    if (menuKey == getMenuKeySymbols()[idx].name)
      menuKeyChoice->value(idx + 1);

  /* Display */
  if (!fullScreen) {
    windowedButton->setonly();
  } else {
    if (fullScreenMode == "all") {
      allMonitorsButton->setonly();
    } else if (fullScreenMode == "selected") {
      selectedMonitorsButton->setonly();
    } else {
      currentMonitorButton->setonly();
    }
  }

  monitorArrangement->value(fullScreenSelectedMonitors.getMonitors());

  handleFullScreenMode(selectedMonitorsButton, this);

  /* Misc. */
  sharedCheckbox->value(shared);
  reconnectCheckbox->value(reconnectOnError);
  alwaysCursorCheckbox->value(alwaysCursor);
  if (cursorType == "system") {
    cursorTypeChoice->value(1);
  } else {
    // Default
    cursorTypeChoice->value(0);
  }
  handleAlwaysCursor(alwaysCursorCheckbox, this);
}


void OptionsDialog::createCompressionPage(int tx, int ty, int tw, int th)
{
  Fl_Group *group = new Fl_Group(tx, ty, tw, th, _("Compression"));

  int orig_tx, orig_ty;
  int col1_ty, col2_ty;
  int half_width, full_width;

  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  full_width = tw - OUTER_MARGIN * 2;
  half_width = (full_width - INNER_MARGIN) / 2;

  /* AutoSelect checkbox */
  autoselectCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                     CHECK_MIN_WIDTH,
                                                     CHECK_HEIGHT,
                                                     _("Auto select")));
  autoselectCheckbox->callback(handleAutoselect, this);
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
    tightButton->callback(handleEncoding, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    zrleButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                              RADIO_MIN_WIDTH,
                                              RADIO_HEIGHT,
                                              "ZRLE"));
    zrleButton->type(FL_RADIO_BUTTON);
    zrleButton->callback(handleEncoding, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    hextileButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                 RADIO_MIN_WIDTH,
                                                 RADIO_HEIGHT,
                                                 "Hextile"));
    hextileButton->type(FL_RADIO_BUTTON);
    hextileButton->callback(handleEncoding, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

#ifdef HAVE_H264
    h264Button = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                             RADIO_MIN_WIDTH,
                                             RADIO_HEIGHT,
                                             "H.264"));
    h264Button->type(FL_RADIO_BUTTON);
    h264Button->callback(handleEncoding, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;
#endif

    rawButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                             RADIO_MIN_WIDTH,
                                             RADIO_HEIGHT,
                                             "Raw"));
    rawButton->type(FL_RADIO_BUTTON);
    rawButton->callback(handleEncoding, this);
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
    fullcolorCheckbox->callback(handleColor, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    mediumcolorCheckbox = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                       RADIO_MIN_WIDTH,
                                                       RADIO_HEIGHT,
                                                       _("Medium")));
    mediumcolorCheckbox->type(FL_RADIO_BUTTON);
    mediumcolorCheckbox->callback(handleColor, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    lowcolorCheckbox = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                    RADIO_MIN_WIDTH,
                                                    RADIO_HEIGHT,
                                                    _("Low")));
    lowcolorCheckbox->type(FL_RADIO_BUTTON);
    lowcolorCheckbox->callback(handleColor, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    verylowcolorCheckbox = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                        RADIO_MIN_WIDTH,
                                                        RADIO_HEIGHT,
                                                        _("Very low")));
    verylowcolorCheckbox->type(FL_RADIO_BUTTON);
    verylowcolorCheckbox->callback(handleColor, this);
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

  /* Checkboxes */
  compressionCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                     CHECK_MIN_WIDTH,
                                                     CHECK_HEIGHT,
                                                     _("Custom compression level:")));
  compressionCheckbox->labelfont(FL_BOLD);
  compressionCheckbox->callback(handleCompression, this);
  ty += CHECK_HEIGHT + TIGHT_MARGIN;

  compressionInput = new Fl_Int_Input(tx + INDENT, ty,
                                      INPUT_HEIGHT, INPUT_HEIGHT,
                                      _("level (0=fast, 9=best)"));
  compressionInput->align(FL_ALIGN_RIGHT);
  compressionInput->callback(handleIntParam, &compressLevel);
  ty += INPUT_HEIGHT + INNER_MARGIN;

  jpegCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                              CHECK_MIN_WIDTH,
                                              CHECK_HEIGHT,
                                              _("Allow JPEG compression:")));
  jpegCheckbox->labelfont(FL_BOLD);
  jpegCheckbox->callback(handleJpeg, this);
  ty += CHECK_HEIGHT + TIGHT_MARGIN;

  jpegInput = new Fl_Int_Input(tx + INDENT, ty,
                               INPUT_HEIGHT, INPUT_HEIGHT,
                               _("quality (0=poor, 9=best)"));
  jpegInput->align(FL_ALIGN_RIGHT);
  jpegInput->callback(handleIntParam, &qualityLevel);
  ty += INPUT_HEIGHT + INNER_MARGIN;

  group->end();
}


void OptionsDialog::createSecurityPage(int tx, int ty, int tw, int th)
{
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
  Fl_Group *group = new Fl_Group(tx, ty, tw, th, _("Security"));

  int orig_tx;
  int width;

  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  width = tw - OUTER_MARGIN * 2;

  orig_tx = tx;

  /* Encryption */
  ty += GROUP_LABEL_OFFSET;
  encryptionGroup = new Fl_Group(tx, ty, width, 0, _("Encryption"));
  encryptionGroup->labelfont(FL_BOLD);
  encryptionGroup->box(FL_FLAT_BOX);
  encryptionGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    encNoneCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                   CHECK_MIN_WIDTH,
                                                   CHECK_HEIGHT,
                                                   _("None")));
    encNoneCheckbox->callback(handleAuth, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

#ifdef HAVE_GNUTLS
    encTLSCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                  CHECK_MIN_WIDTH,
                                                  CHECK_HEIGHT,
                                                  _("TLS with anonymous certificates")));
    encTLSCheckbox->callback(handleAuth, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    encX509Checkbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                   CHECK_MIN_WIDTH,
                                                   CHECK_HEIGHT,
                                                   _("TLS with X509 certificates")));
    encX509Checkbox->callback(handleX509, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    ty += INPUT_LABEL_OFFSET;
    caInput = new Fl_Input(tx + INDENT, ty, 
                           width - INDENT * 2, INPUT_HEIGHT,
                           _("Path to X509 CA certificate"));
    caInput->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);
    caInput->callback(handleStringParam, &rfb::CSecurityTLS::X509CA);
    ty += INPUT_HEIGHT + TIGHT_MARGIN;

    ty += INPUT_LABEL_OFFSET;
    crlInput = new Fl_Input(tx + INDENT, ty,
                            width - INDENT * 2, INPUT_HEIGHT,
                            _("Path to X509 CRL file"));
    crlInput->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);
    crlInput->callback(handleStringParam, &rfb::CSecurityTLS::X509CRL);
    ty += INPUT_HEIGHT + TIGHT_MARGIN;
#endif
#ifdef HAVE_NETTLE
    encRSAAESCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                     CHECK_MIN_WIDTH,
                                                     CHECK_HEIGHT,
                                                     "RSA-AES"));
    encRSAAESCheckbox->callback(handleRSAAES, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;
#endif
  }

  ty -= TIGHT_MARGIN;

  encryptionGroup->end();
  /* Needed for resize to work sanely */
  encryptionGroup->resizable(nullptr);
  encryptionGroup->size(encryptionGroup->w(),
                        ty - encryptionGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  /* Authentication */
  ty += GROUP_LABEL_OFFSET;
  authenticationGroup = new Fl_Group(tx, ty, width, 0, _("Authentication"));
  authenticationGroup->labelfont(FL_BOLD);
  authenticationGroup->box(FL_FLAT_BOX);
  authenticationGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    authNoneCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                    CHECK_MIN_WIDTH,
                                                    CHECK_HEIGHT,
                                                    _("None")));
    authNoneCheckbox->callback(handleAuth, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    authVncCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                   CHECK_MIN_WIDTH,
                                                   CHECK_HEIGHT,
                                                   _("Standard VNC (insecure without encryption)")));
    authVncCheckbox->callback(handleAuth, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    authPlainCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                     CHECK_MIN_WIDTH,
                                                     CHECK_HEIGHT,
                                                     _("Username and password (insecure without encryption)")));
    authPlainCheckbox->callback(handleAuth, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;
  }

  ty -= TIGHT_MARGIN;

  authenticationGroup->end();
  /* Needed for resize to work sanely */
  authenticationGroup->resizable(nullptr);
  authenticationGroup->size(authenticationGroup->w(),
                            ty - authenticationGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  group->end();
#else
  (void)tx;
  (void)ty;
  (void)tw;
  (void)th;
#endif
}


void OptionsDialog::createInputPage(int tx, int ty, int tw, int th)
{
  Fl_Group *group = new Fl_Group(tx, ty, tw, th, _("Input"));

  int orig_tx;
  int width;

  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  width = tw - OUTER_MARGIN * 2;

  viewOnlyCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                  CHECK_MIN_WIDTH,
                                                  CHECK_HEIGHT,
                                                  _("View only (ignore mouse and keyboard)")));
  viewOnlyCheckbox->callback(handleBoolParam, &viewOnly);
  ty += CHECK_HEIGHT + INNER_MARGIN;

  orig_tx = tx;

  /* Mouse */
  ty += GROUP_LABEL_OFFSET;
  mouseGroup = new Fl_Group(tx, ty, width, 0, _("Mouse"));
  mouseGroup->labelfont(FL_BOLD);
  mouseGroup->box(FL_FLAT_BOX);
  mouseGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    emulateMBCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                     CHECK_MIN_WIDTH,
                                                     CHECK_HEIGHT,
                                                     _("Emulate middle mouse button")));
    emulateMBCheckbox->callback(handleBoolParam, &emulateMiddleButton);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    alwaysCursorCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                       CHECK_MIN_WIDTH,
                                                       CHECK_HEIGHT,
                                                       _("Show local cursor when not provided by server")));
    alwaysCursorCheckbox->callback(handleAlwaysCursor, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    /* Cursor type */
    cursorTypeChoice = new Fl_Choice(LBLLEFT(tx, ty, 150, CHOICE_HEIGHT, _("Cursor type")));
    cursorTypeChoice->callback(handleCursorType, this);

    fltk_menu_add(cursorTypeChoice, _("Dot"), 0, nullptr, nullptr, 0);
    fltk_menu_add(cursorTypeChoice, _("System"), 0, nullptr, nullptr, 0);

    fltk_adjust_choice(cursorTypeChoice);

    ty += CHOICE_HEIGHT + TIGHT_MARGIN;

  }
  ty -= TIGHT_MARGIN;

  mouseGroup->end();
  /* Needed for resize to work sanely */
  mouseGroup->resizable(nullptr);
  mouseGroup->size(mouseGroup->w(), ty - mouseGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  /* Keyboard */
  ty += GROUP_LABEL_OFFSET;
  keyboardGroup = new Fl_Group(tx, ty, width, 0, _("Keyboard"));
  keyboardGroup->labelfont(FL_BOLD);
  keyboardGroup->box(FL_FLAT_BOX);
  keyboardGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    systemKeysCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                      CHECK_MIN_WIDTH,
                                                      CHECK_HEIGHT,
                                                      _("Pass system keys directly to server (full screen)")));
    systemKeysCheckbox->callback(handleBoolParam, &fullscreenSystemKeys);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    menuKeyChoice = new Fl_Choice(LBLLEFT(tx, ty, 150, CHOICE_HEIGHT, _("Menu key")));
    menuKeyChoice->callback(handleMenuKey, this);

    fltk_menu_add(menuKeyChoice, _("None"), 0, nullptr, nullptr, FL_MENU_DIVIDER);
    for (int idx = 0; idx < getMenuKeySymbolCount(); idx++)
      fltk_menu_add(menuKeyChoice, getMenuKeySymbols()[idx].name, 0, nullptr, nullptr, 0);

    fltk_adjust_choice(menuKeyChoice);

    ty += CHOICE_HEIGHT + TIGHT_MARGIN;
  }
  ty -= TIGHT_MARGIN;

  keyboardGroup->end();
  /* Needed for resize to work sanely */
  keyboardGroup->resizable(nullptr);
  keyboardGroup->size(keyboardGroup->w(), ty - keyboardGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  /* Clipboard */
  ty += GROUP_LABEL_OFFSET;
  clipboardGroup = new Fl_Group(tx, ty, width, 0, _("Clipboard"));
  clipboardGroup->labelfont(FL_BOLD);
  clipboardGroup->box(FL_FLAT_BOX);
  clipboardGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    acceptClipboardCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                           CHECK_MIN_WIDTH,
                                                           CHECK_HEIGHT,
                                                           _("Accept clipboard from server")));
    acceptClipboardCheckbox->callback(handleClipboard, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

#if !defined(WIN32) && !defined(__APPLE__)
    setPrimaryCheckbox = new Fl_Check_Button(LBLRIGHT(tx + INDENT, ty,
                                                      CHECK_MIN_WIDTH,
                                                      CHECK_HEIGHT,
                                                      _("Also set primary selection")));
    setPrimaryCheckbox->callback(handleBoolParam, &setPrimary);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;
#endif

    sendClipboardCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                         CHECK_MIN_WIDTH,
                                                         CHECK_HEIGHT,
                                                         _("Send clipboard to server")));
    sendClipboardCheckbox->callback(handleClipboard, this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

#if !defined(WIN32) && !defined(__APPLE__)
    sendPrimaryCheckbox = new Fl_Check_Button(LBLRIGHT(tx + INDENT, ty,
                                                       CHECK_MIN_WIDTH,
                                                       CHECK_HEIGHT,
                                                       _("Send primary selection as clipboard")));
    sendPrimaryCheckbox->callback(handleBoolParam, &sendPrimary);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;
#endif
  }
  ty -= TIGHT_MARGIN;

  clipboardGroup->end();
  /* Needed for resize to work sanely */
  clipboardGroup->resizable(nullptr);
  clipboardGroup->size(clipboardGroup->w(), ty - clipboardGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  group->end();
}


void OptionsDialog::createDisplayPage(int tx, int ty, int tw, int th)
{
  Fl_Group *group = new Fl_Group(tx, ty, tw, th, _("Display"));

  int orig_tx;
  int width;

  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  width = tw - OUTER_MARGIN * 2;

  orig_tx = tx;

  /* Display mode */
  ty += GROUP_LABEL_OFFSET;
  displayModeGroup = new Fl_Group(tx, ty, width, 0, _("Display mode"));
  displayModeGroup->labelfont(FL_BOLD);
  displayModeGroup->box(FL_FLAT_BOX);
  displayModeGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;
    width -= INDENT;

    windowedButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                  RADIO_MIN_WIDTH,
                                                  RADIO_HEIGHT,
                                                  _("Windowed")));
    windowedButton->type(FL_RADIO_BUTTON);
    windowedButton->callback(handleFullScreenMode, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    currentMonitorButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                        RADIO_MIN_WIDTH,
                                                        RADIO_HEIGHT,
                                                        _("Full screen on current monitor")));
    currentMonitorButton->type(FL_RADIO_BUTTON);
    currentMonitorButton->callback(handleFullScreenMode, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    allMonitorsButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                            RADIO_MIN_WIDTH,
                                            RADIO_HEIGHT,
                                            _("Full screen on all monitors")));
    allMonitorsButton->type(FL_RADIO_BUTTON);
    allMonitorsButton->callback(handleFullScreenMode, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    selectedMonitorsButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                 RADIO_MIN_WIDTH,
                                                 RADIO_HEIGHT,
                                                 _("Full screen on selected monitor(s)")));
    selectedMonitorsButton->type(FL_RADIO_BUTTON);
    selectedMonitorsButton->callback(handleFullScreenMode, this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    monitorArrangement = new Fl_Monitor_Arrangement(
                              tx + INDENT, ty,
                              width - INDENT, 150);
    monitorArrangement->callback(handleFullScreenMode, this);
    ty += 150 + TIGHT_MARGIN;
  }
  ty -= TIGHT_MARGIN;

  displayModeGroup->end();
  /* Needed for resize to work sanely */
  displayModeGroup->resizable(nullptr);
  displayModeGroup->size(displayModeGroup->w(),
                         ty - displayModeGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;
  width = tw - OUTER_MARGIN * 2;

  group->end();
}


void OptionsDialog::createMiscPage(int tx, int ty, int tw, int th)
{
  Fl_Group *group = new Fl_Group(tx, ty, tw, th, _("Miscellaneous"));

  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  sharedCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                  CHECK_MIN_WIDTH,
                                                  CHECK_HEIGHT,
                                                  _("Shared (don't disconnect other viewers)")));
  sharedCheckbox->callback(handleBoolParam, &shared);
  ty += CHECK_HEIGHT + TIGHT_MARGIN;

  reconnectCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                  CHECK_MIN_WIDTH,
                                                  CHECK_HEIGHT,
                                                  _("Ask to reconnect on connection errors")));
  reconnectCheckbox->callback(handleBoolParam, &reconnectOnError);
  ty += CHECK_HEIGHT + TIGHT_MARGIN;

  group->end();
}


void OptionsDialog::handleBoolParam(Fl_Widget *widget, void *data)
{
  Fl_Check_Button *check;
  core::BoolParameter *param;

  check = dynamic_cast<Fl_Check_Button*>(widget);
  param = dynamic_cast<core::BoolParameter*>((core::VoidParameter*)data);

  assert(check);
  assert(param);

  param->setParam(check->value());
}


void OptionsDialog::handleIntParam(Fl_Widget *widget, void *data)
{
  Fl_Int_Input *input;
  core::IntParameter *param;

  input = dynamic_cast<Fl_Int_Input*>(widget);
  param = dynamic_cast<core::IntParameter*>((core::VoidParameter*)data);

  assert(input);
  assert(param);

  param->setParam(input->value());
}


void OptionsDialog::handleStringParam(Fl_Widget *widget, void *data)
{
  Fl_Input *input;
  core::StringParameter *param;

  input = dynamic_cast<Fl_Input*>(widget);
  param = dynamic_cast<core::StringParameter*>((core::VoidParameter*)data);

  assert(input);
  assert(param);

  param->setParam(input->value());
}


void OptionsDialog::handleAutoselect(Fl_Widget *widget, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->autoselectCheckbox->value()) {
    dialog->encodingGroup->deactivate();
    dialog->colorlevelGroup->deactivate();
  } else {
    dialog->encodingGroup->activate();
    dialog->colorlevelGroup->activate();
  }

  handleBoolParam(widget, &autoSelect);

  // JPEG setting is also affected by autoselection
  dialog->handleJpeg(dialog->jpegCheckbox, dialog);
}


void OptionsDialog::handleEncoding(Fl_Widget* /*widget*/, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->tightButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingTight));
  else if (dialog->zrleButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingZRLE));
  else if (dialog->hextileButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingHextile));
#ifdef HAVE_H264
  else if (dialog->h264Button->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingH264));
#endif
  else if (dialog->rawButton->value())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingRaw));
}


void OptionsDialog::handleColor(Fl_Widget* /*widget*/, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  fullColour.setParam(dialog->fullcolorCheckbox->value());
  if (dialog->verylowcolorCheckbox->value())
    lowColourLevel.setParam(0);
  else if (dialog->lowcolorCheckbox->value())
    lowColourLevel.setParam(1);
  else if (dialog->mediumcolorCheckbox->value())
    lowColourLevel.setParam(2);
}


void OptionsDialog::handleCompression(Fl_Widget *widget, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->compressionCheckbox->value())
    dialog->compressionInput->activate();
  else
    dialog->compressionInput->deactivate();

  handleBoolParam(widget, &customCompressLevel);
}


void OptionsDialog::handleJpeg(Fl_Widget* /*widget*/, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->jpegCheckbox->value() &&
      !dialog->autoselectCheckbox->value())
    dialog->jpegInput->activate();
  else
    dialog->jpegInput->deactivate();

  noJpeg.setParam(!dialog->jpegCheckbox->value());
}


void OptionsDialog::handleAuth(Fl_Widget* /*widget*/, void *data)
{
  (void)data;
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
  OptionsDialog *dialog = (OptionsDialog*)data;
  rfb::Security security;

  /* Process security types which don't use encryption */
  if (dialog->encNoneCheckbox->value()) {
    if (dialog->authNoneCheckbox->value())
      security.EnableSecType(rfb::secTypeNone);
    if (dialog->authVncCheckbox->value()) {
      security.EnableSecType(rfb::secTypeVncAuth);
#ifdef HAVE_NETTLE
      security.EnableSecType(rfb::secTypeRA2ne);
      security.EnableSecType(rfb::secTypeRAne256);
#endif
    }
    if (dialog->authPlainCheckbox->value()) {
      security.EnableSecType(rfb::secTypePlain);
#ifdef HAVE_NETTLE
      security.EnableSecType(rfb::secTypeRA2ne);
      security.EnableSecType(rfb::secTypeRAne256);
      security.EnableSecType(rfb::secTypeDH);
      security.EnableSecType(rfb::secTypeMSLogonII);
#endif
    }
  }

#ifdef HAVE_GNUTLS
  /* Process security types which use TLS encryption */
  if (dialog->encTLSCheckbox->value()) {
    if (dialog->authNoneCheckbox->value())
      security.EnableSecType(rfb::secTypeTLSNone);
    if (dialog->authVncCheckbox->value())
      security.EnableSecType(rfb::secTypeTLSVnc);
    if (dialog->authPlainCheckbox->value())
      security.EnableSecType(rfb::secTypeTLSPlain);
  }

  /* Process security types which use X509 encryption */
  if (dialog->encX509Checkbox->value()) {
    if (dialog->authNoneCheckbox->value())
      security.EnableSecType(rfb::secTypeX509None);
    if (dialog->authVncCheckbox->value())
      security.EnableSecType(rfb::secTypeX509Vnc);
    if (dialog->authPlainCheckbox->value())
      security.EnableSecType(rfb::secTypeX509Plain);
  }
#endif

#ifdef HAVE_NETTLE
  if (dialog->encRSAAESCheckbox->value()) {
    security.EnableSecType(rfb::secTypeRA2);
    security.EnableSecType(rfb::secTypeRA256);
  }
#endif

  rfb::SecurityClient::secTypes.setParam(security.ToString());
#endif
}


void OptionsDialog::handleX509(Fl_Widget *widget, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->encX509Checkbox->value()) {
    dialog->caInput->activate();
    dialog->crlInput->activate();
  } else {
    dialog->caInput->deactivate();
    dialog->crlInput->deactivate();
  }

  handleAuth(widget, data);
}


void OptionsDialog::handleRSAAES(Fl_Widget *widget, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->encRSAAESCheckbox->value()) {
    dialog->authVncCheckbox->value(true);
    dialog->authPlainCheckbox->value(true);
  }

  handleAuth(widget, data);
}


void OptionsDialog::handleMenuKey(Fl_Widget* /*widget*/, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->menuKeyChoice->value() == 0)
    menuKey.setParam("None");
  else {
    menuKey.setParam(dialog->menuKeyChoice->text());
  }
}


void OptionsDialog::handleClipboard(Fl_Widget* /*widget*/, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

#if !defined(WIN32) && !defined(__APPLE__)

  if (dialog->acceptClipboardCheckbox->value())
    dialog->setPrimaryCheckbox->activate();
  else
    dialog->setPrimaryCheckbox->deactivate();
  if (dialog->sendClipboardCheckbox->value())
    dialog->sendPrimaryCheckbox->activate();
  else
    dialog->sendPrimaryCheckbox->deactivate();
#endif

  handleBoolParam(dialog->acceptClipboardCheckbox, &acceptClipboard);
  handleBoolParam(dialog->sendClipboardCheckbox, &sendClipboard);
}

void OptionsDialog::handleFullScreenMode(Fl_Widget* /*widget*/, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->selectedMonitorsButton->value()) {
    dialog->monitorArrangement->activate();
  } else {
    dialog->monitorArrangement->deactivate();
  }

  if (dialog->windowedButton->value()) {
    fullScreen.setParam(false);
  } else {
    if (dialog->allMonitorsButton->value()) {
      fullScreenMode.setParam("All");
    } else if (dialog->selectedMonitorsButton->value()) {
      fullScreenMode.setParam("Selected");
    } else {
      fullScreenMode.setParam("Current");
    }

    fullScreen.setParam(true);
  }

  fullScreenSelectedMonitors.setMonitors(dialog->monitorArrangement->value());
}


int OptionsDialog::fltk_event_handler(int event)
{
  std::set<OptionsDialog *>::iterator iter;

  if (event != FL_SCREEN_CONFIGURATION_CHANGED)
    return 0;

  // Refresh monitor arrangement widget to match the parameter settings after
  // screen configuration has changed. The MonitorArrangement index doesn't work
  // the same way as the FLTK screen index.
  for (iter = instances.begin(); iter != instances.end(); iter++)
      Fl::add_timeout(0, handleScreenConfigTimeout, (*iter));

  return 0;
}

void OptionsDialog::handleScreenConfigTimeout(void *data)
{
    OptionsDialog *self = (OptionsDialog *)data;

    assert(self);

    self->monitorArrangement->value(fullScreenSelectedMonitors.getMonitors());
}

void OptionsDialog::handleAlwaysCursor(Fl_Widget* /*widget*/, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->alwaysCursorCheckbox->value()) {
    dialog->cursorTypeChoice->activate();
  } else {
    dialog->cursorTypeChoice->deactivate();
  }

  handleBoolParam(dialog->alwaysCursorCheckbox, &alwaysCursor);
}

void OptionsDialog::handleCursorType(Fl_Widget* /*widget*/, void *data)
{
  OptionsDialog *dialog = (OptionsDialog*)data;

  if (dialog->alwaysCursorCheckbox->value()) {
    if (dialog->cursorTypeChoice->value() == 1) {
      cursorType.setParam("System");
    } else {
      // Default
      cursorType.setParam("Dot");
    }
  }
}
