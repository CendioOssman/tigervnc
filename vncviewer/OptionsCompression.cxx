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

#include "OptionsCompression.h"
#include "parameters.h"
#include "rfb/encodings.h"
#include "i18n.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

OptionsCompression::OptionsCompression(QWidget* parent)
  : OptionsPage{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  autoselectCheckbox = new QCheckBox(_("Auto select"));
  layout->addWidget(autoselectCheckbox);

  QHBoxLayout* hLayout = new QHBoxLayout;

  QGroupBox* groupBox1 = new QGroupBox(_("Preferred encoding"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  tightButton = new QRadioButton(_("Tight"));
  vbox1->addWidget(tightButton);
  zrleButton = new QRadioButton(_("ZRLE"));
  vbox1->addWidget(zrleButton);
  hextileButton = new QRadioButton(_("Hextile"));
  vbox1->addWidget(hextileButton);
  h264Button = new QRadioButton(_("H264"));
#ifndef HAVE_H264
  h264Button->setVisible(false);
#endif
  vbox1->addWidget(h264Button);
  rawButton = new QRadioButton(_("Raw"));
  vbox1->addWidget(rawButton);
  groupBox1->setLayout(vbox1);
  hLayout->addWidget(groupBox1, 1);

  QGroupBox* groupBox2 = new QGroupBox(_("Color level"));
  QVBoxLayout* vbox2 = new QVBoxLayout;
  fullcolorCheckbox = new QRadioButton(_("Full"));
  vbox2->addWidget(fullcolorCheckbox);
  mediumcolorCheckbox = new QRadioButton(_("Medium"));
  vbox2->addWidget(mediumcolorCheckbox);
  lowcolorCheckbox = new QRadioButton(_("Low"));
  vbox2->addWidget(lowcolorCheckbox);
  verylowcolorCheckbox = new QRadioButton(_("Very Low"));
  vbox2->addWidget(verylowcolorCheckbox);
  groupBox2->setLayout(vbox2);
  hLayout->addWidget(groupBox2, 1);

  layout->addLayout(hLayout);

  compressionCheckbox = new QCheckBox(_("Custom compression level:"));
  layout->addWidget(compressionCheckbox);
  QHBoxLayout* h1 = new QHBoxLayout;
  h1->addSpacing(20);
  compressionInput = new QSpinBox;
  compressionInput->setRange(0, 9);
  compressionInput->setFixedWidth(80);
  h1->addWidget(compressionInput);
  h1->addWidget(new QLabel(_("level (0=fast, 9=best)")));
  h1->addStretch(1);
  layout->addLayout(h1);
  jpegCheckbox = new QCheckBox(_("Allow JPEG compression quality:"));
  layout->addWidget(jpegCheckbox);
  QHBoxLayout* h2 = new QHBoxLayout;
  h2->addSpacing(20);
  jpegInput = new QSpinBox;
  jpegInput->setRange(0, 9);
  jpegInput->setFixedWidth(80);
  h2->addWidget(jpegInput);
  h2->addWidget(new QLabel(_("quality (0=poor, 9=best)")));
  h2->addStretch(1);
  layout->addLayout(h2);

  layout->addStretch(1);

  setLayout(layout);

  connect(autoselectCheckbox, &QCheckBox::toggled, this, [=](bool checked) {
    groupBox1->setEnabled(!checked);
    groupBox2->setEnabled(!checked);
    jpegInput->setEnabled(!checked && jpegCheckbox->isChecked());
  });
  connect(compressionCheckbox, &QCheckBox::toggled, this, [=](bool checked) {
    compressionInput->setEnabled(checked);
  });
  connect(jpegCheckbox, &QCheckBox::toggled, this, [=](bool checked) {
    jpegInput->setEnabled(!autoselectCheckbox->isChecked() && checked);
  });
}

void OptionsCompression::apply()
{
  ::autoSelect.setParam(autoselectCheckbox->isChecked());
  if (tightButton->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(7));
  }
  if (zrleButton->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(16));
  }
  if (hextileButton->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(5));
  }
  if (h264Button->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(50));
  }
  if (rawButton->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(0));
  }
  ::fullColour.setParam(fullcolorCheckbox->isChecked());
  if (mediumcolorCheckbox->isChecked()) {
    ::lowColourLevel.setParam(2);
  }
  if (lowcolorCheckbox->isChecked()) {
    ::lowColourLevel.setParam(1);
  }
  if (verylowcolorCheckbox->isChecked()) {
    ::lowColourLevel.setParam(0);
  }
  ::customCompressLevel.setParam(compressionCheckbox->isChecked());
  ::compressLevel.setParam(compressionInput->value());
  ::noJpeg.setParam(!jpegCheckbox->isChecked());
  ::qualityLevel.setParam(jpegInput->value());
}

void OptionsCompression::reset()
{
  autoselectCheckbox->setChecked(::autoSelect);
  tightButton->setChecked(rfb::encodingNum(::preferredEncoding) == 7);
  zrleButton->setChecked(rfb::encodingNum(::preferredEncoding) == 16);
  hextileButton->setChecked(rfb::encodingNum(::preferredEncoding) == 5);
  h264Button->setChecked(rfb::encodingNum(::preferredEncoding) == 50);
  rawButton->setChecked(rfb::encodingNum(::preferredEncoding) == 0);
  mediumcolorCheckbox->setChecked(::lowColourLevel == 2);
  lowcolorCheckbox->setChecked(::lowColourLevel == 1);
  verylowcolorCheckbox->setChecked(::lowColourLevel == 0);
  fullcolorCheckbox->setChecked(::fullColour);
  compressionCheckbox->setChecked(::customCompressLevel);
  compressionInput->setValue(::compressLevel);
  jpegCheckbox->setChecked(!::noJpeg);
  jpegInput->setValue(::qualityLevel);
}
