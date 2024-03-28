#include "compressiontab.h"

#include "parameters.h"

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

  compressionAutoSelect = new QCheckBox(tr("Auto select"));
  layout->addWidget(compressionAutoSelect);

  QHBoxLayout* hLayout = new QHBoxLayout;

  QGroupBox* groupBox1 = new QGroupBox(tr("Preferred encoding"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  compressionEncodingTight = new QRadioButton(tr("Tight"));
  vbox1->addWidget(compressionEncodingTight);
  compressionEncodingZRLE = new QRadioButton(tr("ZRLE"));
  vbox1->addWidget(compressionEncodingZRLE);
  compressionEncodingHextile = new QRadioButton(tr("Hextile"));
  vbox1->addWidget(compressionEncodingHextile);
  compressionEncodingH264 = new QRadioButton(tr("H264"));
  compressionEncodingH264->setEnabled(ViewerConfig::config()->haveH264());
  vbox1->addWidget(compressionEncodingH264);
  compressionEncodingRaw = new QRadioButton(tr("Raw"));
  vbox1->addWidget(compressionEncodingRaw);
  groupBox1->setLayout(vbox1);
  hLayout->addWidget(groupBox1, 1);

  QGroupBox* groupBox2 = new QGroupBox(tr("Color level"));
  QVBoxLayout* vbox2 = new QVBoxLayout;
  compressionColorLevelFull = new QRadioButton(tr("Full"));
  vbox2->addWidget(compressionColorLevelFull);
  compressionColorLevelMedium = new QRadioButton(tr("Medium"));
  vbox2->addWidget(compressionColorLevelMedium);
  compressionColorLevelLow = new QRadioButton(tr("Low"));
  vbox2->addWidget(compressionColorLevelLow);
  compressionColorLevelVeryLow = new QRadioButton(tr("Very Low"));
  vbox2->addWidget(compressionColorLevelVeryLow);
  groupBox2->setLayout(vbox2);
  hLayout->addWidget(groupBox2, 1);

  layout->addLayout(hLayout);

  compressionCustomCompressionLevel = new QCheckBox(tr("Custom compression level (0=fast, 9=best)"));
  layout->addWidget(compressionCustomCompressionLevel);
  compressionCustomCompressionLevelTextEdit = new QSpinBox;
  compressionCustomCompressionLevelTextEdit->setRange(0, 9);
  layout->addWidget(compressionCustomCompressionLevelTextEdit);
  compressionJPEGCompression = new QCheckBox(tr("Allow JPEG compression quality (0=poor, 9=best)"));
  layout->addWidget(compressionJPEGCompression);
  compressionJPEGCompressionTextEdit = new QSpinBox;
  compressionJPEGCompressionTextEdit->setRange(0, 9);
  layout->addWidget(compressionJPEGCompressionTextEdit);

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
  ViewerConfig::config()->setAutoSelect(compressionAutoSelect->isChecked());
  if (compressionEncodingTight->isChecked()) {
    ViewerConfig::config()->setPreferredEncodingNum(7);
  }
  if (compressionEncodingZRLE->isChecked()) {
    ViewerConfig::config()->setPreferredEncodingNum(16);
  }
  if (compressionEncodingHextile->isChecked()) {
    ViewerConfig::config()->setPreferredEncodingNum(5);
  }
  if (compressionEncodingH264->isChecked()) {
    ViewerConfig::config()->setPreferredEncodingNum(50);
  }
  if (compressionEncodingRaw->isChecked()) {
    ViewerConfig::config()->setPreferredEncodingNum(0);
  }
  ViewerConfig::config()->setFullColour(compressionColorLevelFull->isChecked());
  if (compressionColorLevelMedium->isChecked()) {
    ViewerConfig::config()->setLowColourLevel(2);
  }
  if (compressionColorLevelLow->isChecked()) {
    ViewerConfig::config()->setLowColourLevel(1);
  }
  if (compressionColorLevelVeryLow->isChecked()) {
    ViewerConfig::config()->setLowColourLevel(0);
  }
  ViewerConfig::config()->setCustomCompressLevel(compressionCustomCompressionLevel->isChecked());
  ViewerConfig::config()->setCompressLevel(compressionCustomCompressionLevelTextEdit->value());
  ViewerConfig::config()->setNoJpeg(!compressionJPEGCompression->isChecked());
  ViewerConfig::config()->setQualityLevel(compressionJPEGCompressionTextEdit->value());
}

void CompressionTab::reset()
{
  compressionAutoSelect->setChecked(ViewerConfig::config()->autoSelect());
  compressionEncodingTight->setChecked(ViewerConfig::config()->preferredEncodingNum() == 7);
  compressionEncodingZRLE->setChecked(ViewerConfig::config()->preferredEncodingNum() == 16);
  compressionEncodingHextile->setChecked(ViewerConfig::config()->preferredEncodingNum() == 5);
  compressionEncodingH264->setChecked(ViewerConfig::config()->preferredEncodingNum() == 50);
  compressionEncodingRaw->setChecked(ViewerConfig::config()->preferredEncodingNum() == 0);
  compressionColorLevelFull->setChecked(ViewerConfig::config()->fullColour());
  compressionColorLevelMedium->setChecked(ViewerConfig::config()->lowColourLevel() == 2);
  compressionColorLevelLow->setChecked(ViewerConfig::config()->lowColourLevel() == 1);
  compressionColorLevelVeryLow->setChecked(ViewerConfig::config()->lowColourLevel() == 0);
  compressionCustomCompressionLevel->setChecked(ViewerConfig::config()->customCompressLevel());
  compressionCustomCompressionLevelTextEdit->setValue(ViewerConfig::config()->compressLevel());
  compressionJPEGCompression->setChecked(!ViewerConfig::config()->noJpeg());
  compressionJPEGCompressionTextEdit->setValue(ViewerConfig::config()->qualityLevel());
}
