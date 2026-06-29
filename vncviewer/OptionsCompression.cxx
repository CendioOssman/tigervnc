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

#include <QBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>

#include <core/Configuration.h>
#include <core/i18n.h>

#include <rfb/CConnection.h>
#include <rfb/encodings.h>

#include "OptionsCompression.h"
#include "parameters.h"

OptionsCompression::OptionsCompression(QWidget* parent)
  : OptionsPage(parent)
{
  QBoxLayout* layout = new QVBoxLayout;

  /* Checkboxes */

  autoselectCheckbox = new QCheckBox(_("Auto select"));
  connect(autoselectCheckbox, &QCheckBox::toggled,
          this, &OptionsCompression::handleAutoselect);
  layout->addWidget(autoselectCheckbox);

  jpegCheckbox = new QCheckBox(_("Allow JPEG compression"));
  layout->addWidget(jpegCheckbox);

  /* Two columns */
  QBoxLayout* columns = new QHBoxLayout;

  /* VNC encoding box */
  encodingGroup = new QGroupBox(_("Preferred encoding"));
  columns->addWidget(encodingGroup, 1);

  {
    QBoxLayout* box = new QVBoxLayout;
    encodingGroup->setLayout(box);

    tightButton = new QRadioButton("Tight");
    box->addWidget(tightButton);

    jpegButton = new QRadioButton("JPEG");
    box->addWidget(jpegButton);

    zrleButton = new QRadioButton("ZRLE");
    box->addWidget(zrleButton);

    hextileButton = new QRadioButton("Hextile");
    box->addWidget(hextileButton);

#ifdef HAVE_H264
    h264Button = new QRadioButton("H264");
    box->addWidget(h264Button);
#endif

    rawButton = new QRadioButton("Raw");
    box->addWidget(rawButton);

    box->addStretch(1);
  }

  /* Color box */
  colorlevelGroup = new QGroupBox(_("Color level"));
  columns->addWidget(colorlevelGroup, 1);

  {
    QBoxLayout* box = new QVBoxLayout;
    colorlevelGroup->setLayout(box);

    fullcolorCheckbox = new QRadioButton(_("Full"));
    box->addWidget(fullcolorCheckbox);

    mediumcolorCheckbox = new QRadioButton(_("Medium"));
    box->addWidget(mediumcolorCheckbox);

    lowcolorCheckbox = new QRadioButton(_("Low"));
    box->addWidget(lowcolorCheckbox);

    verylowcolorCheckbox = new QRadioButton(_("Very Low"));
    box->addWidget(verylowcolorCheckbox);

    box->addStretch(1);
  }

  layout->addLayout(columns);

  /* Compression */
  compressionGroup = new QGroupBox(_("Custom compression level:"));
  compressionGroup->setCheckable(true);
  layout->addWidget(compressionGroup);

  {
    QBoxLayout* box = new QVBoxLayout;
    compressionGroup->setLayout(box);

    QBoxLayout* compression = new QHBoxLayout;
    compressionInput = new QSpinBox;
    compressionInput->setRange(0, 9);
    compressionInput->setFixedWidth(80);
    compression->addWidget(compressionInput);
    compression->addWidget(new QLabel(_("level (0=fast, 9=best)")));
    compression->addStretch(1);
    box->addLayout(compression);
  }

  /* Quality */
  qualityGroup = new QGroupBox(_("Quality level"));
  layout->addWidget(qualityGroup, 1);

  {
    QBoxLayout* box = new QVBoxLayout;
    qualityGroup->setLayout(box);

    QBoxLayout* jpeg = new QHBoxLayout;
    jpegInput = new QSpinBox;
    jpegInput->setRange(0, 9);
    jpegInput->setFixedWidth(80);
    jpeg->addWidget(jpegInput);
    jpeg->addWidget(new QLabel(_("quality (0=poor, 9=best)")));
    jpeg->addStretch(1);
    box->addLayout(jpeg);
  }

  layout->addStretch(1);

  setLayout(layout);
}

void OptionsCompression::loadOptions()
{
  autoselectCheckbox->setChecked(autoSelect);
  jpegCheckbox->setChecked(!rfb::CConnection::noJpeg);

  if (preferredEncoding == "Tight")
    tightButton->setChecked(true);
  else if (preferredEncoding == "JPEG")
    jpegButton->setChecked(true);
  else if (preferredEncoding == "ZRLE")
    zrleButton->setChecked(true);
  else if (preferredEncoding == "Hextile")
    hextileButton->setChecked(true);
#ifdef HAVE_H264
  else if (preferredEncoding == "H.264")
    h264Button->setChecked(true);
#endif
  else if (preferredEncoding == "Raw")
    rawButton->setChecked(true);

  if (fullColour)
    fullcolorCheckbox->setChecked(true);
  else {
    switch (lowColourLevel) {
    case 0:
      verylowcolorCheckbox->setChecked(true);
      break;
    case 1:
      lowcolorCheckbox->setChecked(true);
      break;
    case 2:
      mediumcolorCheckbox->setChecked(true);
      break;
    }
  }

  compressionGroup->setChecked(customCompressLevel);
  compressionInput->setValue(compressLevel);
  jpegInput->setValue(qualityLevel);
}

void OptionsCompression::storeOptions()
{
  autoSelect.setParam(autoselectCheckbox->isChecked());
  rfb::CConnection::noJpeg.setParam(!jpegCheckbox->isChecked());

  if (tightButton->isChecked())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingTight));
  else if (jpegButton->isChecked())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingJPEG));
  else if (zrleButton->isChecked())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingZRLE));
  else if (hextileButton->isChecked())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingHextile));
#ifdef HAVE_H264
  else if (h264Button->isChecked())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingH264));
#endif
  else if (rawButton->isChecked())
    preferredEncoding.setParam(rfb::encodingName(rfb::encodingRaw));

  fullColour.setParam(fullcolorCheckbox->isChecked());
  if (verylowcolorCheckbox->isChecked())
    lowColourLevel.setParam(0);
  else if (lowcolorCheckbox->isChecked())
    lowColourLevel.setParam(1);
  else if (mediumcolorCheckbox->isChecked())
    lowColourLevel.setParam(2);

  customCompressLevel.setParam(compressionGroup->isChecked());
  compressLevel.setParam(compressionInput->value());
  qualityLevel.setParam(jpegInput->value());
}

void OptionsCompression::handleAutoselect()
{
  bool isAuto;

  isAuto = autoselectCheckbox->isChecked();
  encodingGroup->setEnabled(!isAuto);
  colorlevelGroup->setEnabled(!isAuto);
  qualityGroup->setEnabled(!isAuto);
}
