#include "compressiontab.h"

#include "parameters.h"
#include "viewerconfig.h"
#include "rfb/encodings.h"
#include "i18n.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

CompressionTab::CompressionTab(QWidget* parent)
  : TabElement{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  compressionAutoSelect = new QCheckBox(_("Auto select"));
  layout->addWidget(compressionAutoSelect);

  QHBoxLayout* hLayout = new QHBoxLayout;

  QGroupBox* groupBox1 = new QGroupBox(_("Preferred encoding"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  compressionEncodingTight = new QRadioButton(_("Tight"));
  vbox1->addWidget(compressionEncodingTight);
  compressionEncodingZRLE = new QRadioButton(_("ZRLE"));
  vbox1->addWidget(compressionEncodingZRLE);
  compressionEncodingHextile = new QRadioButton(_("Hextile"));
  vbox1->addWidget(compressionEncodingHextile);
  compressionEncodingH264 = new QRadioButton(_("H264"));
#ifndef HAVE_H264
  compressionEncodingH264->setVisible(false);
#endif
  vbox1->addWidget(compressionEncodingH264);
  compressionEncodingRaw = new QRadioButton(_("Raw"));
  vbox1->addWidget(compressionEncodingRaw);
  groupBox1->setLayout(vbox1);
  hLayout->addWidget(groupBox1, 1);

  QGroupBox* groupBox2 = new QGroupBox(_("Color level"));
  QVBoxLayout* vbox2 = new QVBoxLayout;
  compressionColorLevelFull = new QRadioButton(_("Full"));
  vbox2->addWidget(compressionColorLevelFull);
  compressionColorLevelMedium = new QRadioButton(_("Medium"));
  vbox2->addWidget(compressionColorLevelMedium);
  compressionColorLevelLow = new QRadioButton(_("Low"));
  vbox2->addWidget(compressionColorLevelLow);
  compressionColorLevelVeryLow = new QRadioButton(_("Very Low"));
  vbox2->addWidget(compressionColorLevelVeryLow);
  groupBox2->setLayout(vbox2);
  hLayout->addWidget(groupBox2, 1);

  layout->addLayout(hLayout);

  compressionCustomCompressionLevel = new QCheckBox(_("Custom compression level:"));
  layout->addWidget(compressionCustomCompressionLevel);
  QHBoxLayout* h1 = new QHBoxLayout;
  h1->addSpacing(20);
  compressionCustomCompressionLevelTextEdit = new QSpinBox;
  compressionCustomCompressionLevelTextEdit->setRange(0, 9);
  compressionCustomCompressionLevelTextEdit->setFixedWidth(50);
  h1->addWidget(compressionCustomCompressionLevelTextEdit);
  h1->addWidget(new QLabel(_("level (0=fast, 9=best)")));
  h1->addStretch(1);
  layout->addLayout(h1);
  compressionJPEGCompression = new QCheckBox(_("Allow JPEG compression quality:"));
  layout->addWidget(compressionJPEGCompression);
  QHBoxLayout* h2 = new QHBoxLayout;
  h2->addSpacing(20);
  compressionJPEGCompressionTextEdit = new QSpinBox;
  compressionJPEGCompressionTextEdit->setRange(0, 9);
  compressionJPEGCompressionTextEdit->setFixedWidth(50);
  h2->addWidget(compressionJPEGCompressionTextEdit);
  h2->addWidget(new QLabel(_("quality (0=poor, 9=best)")));
  h2->addStretch(1);
  layout->addLayout(h2);

  layout->addStretch(1);

  setLayout(layout);

  connect(compressionAutoSelect, &QCheckBox::toggled, this, [=](bool checked) {
    groupBox1->setEnabled(!checked);
    groupBox2->setEnabled(!checked);
    compressionJPEGCompressionTextEdit->setEnabled(!checked && compressionJPEGCompression->isChecked());
  });
  connect(compressionCustomCompressionLevel, &QCheckBox::toggled, this, [=](bool checked) {
    compressionCustomCompressionLevelTextEdit->setEnabled(checked);
  });
  connect(compressionJPEGCompression, &QCheckBox::toggled, this, [=](bool checked) {
    compressionJPEGCompressionTextEdit->setEnabled(!compressionAutoSelect->isChecked() && checked);
  });
}

void CompressionTab::apply()
{
  ::autoSelect.setParam(compressionAutoSelect->isChecked());
  if (compressionEncodingTight->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(7));
  }
  if (compressionEncodingZRLE->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(16));
  }
  if (compressionEncodingHextile->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(5));
  }
  if (compressionEncodingH264->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(50));
  }
  if (compressionEncodingRaw->isChecked()) {
    ::preferredEncoding.setParam(rfb::encodingName(0));
  }
  ::fullColour.setParam(compressionColorLevelFull->isChecked());
  if (compressionColorLevelMedium->isChecked()) {
    ::lowColourLevel.setParam(2);
  }
  if (compressionColorLevelLow->isChecked()) {
    ::lowColourLevel.setParam(1);
  }
  if (compressionColorLevelVeryLow->isChecked()) {
    ::lowColourLevel.setParam(0);
  }
  ::customCompressLevel.setParam(compressionCustomCompressionLevel->isChecked());
  ::compressLevel.setParam(compressionCustomCompressionLevelTextEdit->value());
  ::noJpeg.setParam(!compressionJPEGCompression->isChecked());
  ::qualityLevel.setParam(compressionJPEGCompressionTextEdit->value());
}

void CompressionTab::reset()
{
  compressionAutoSelect->setChecked(::autoSelect);
  compressionEncodingTight->setChecked(rfb::encodingNum(::preferredEncoding) == 7);
  compressionEncodingZRLE->setChecked(rfb::encodingNum(::preferredEncoding) == 16);
  compressionEncodingHextile->setChecked(rfb::encodingNum(::preferredEncoding) == 5);
  compressionEncodingH264->setChecked(rfb::encodingNum(::preferredEncoding) == 50);
  compressionEncodingRaw->setChecked(rfb::encodingNum(::preferredEncoding) == 0);
  compressionColorLevelMedium->setChecked(::lowColourLevel == 2);
  compressionColorLevelLow->setChecked(::lowColourLevel == 1);
  compressionColorLevelVeryLow->setChecked(::lowColourLevel == 0);
  compressionColorLevelFull->setChecked(::fullColour);
  compressionCustomCompressionLevel->setChecked(::customCompressLevel);
  compressionCustomCompressionLevelTextEdit->setValue(::compressLevel);
  compressionJPEGCompression->setChecked(!::noJpeg);
  compressionJPEGCompressionTextEdit->setValue(::qualityLevel);
}
