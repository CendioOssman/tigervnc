#ifndef COMPRESSIONTAB_H
#define COMPRESSIONTAB_H

#include "../OptionsDialog.h"

#include <QWidget>

class QCheckBox;
class QSpinBox;
class QRadioButton;

class OptionsCompression : public TabElement
{
  Q_OBJECT

public:
  OptionsCompression(QWidget* parent = nullptr);

  void apply() override;
  void reset() override;

private:
  QCheckBox* compressionAutoSelect;
  QRadioButton* compressionEncodingTight;
  QRadioButton* compressionEncodingZRLE;
  QRadioButton* compressionEncodingHextile;
  QRadioButton* compressionEncodingH264;
  QRadioButton* compressionEncodingRaw;
  QRadioButton* compressionColorLevelFull;
  QRadioButton* compressionColorLevelMedium;
  QRadioButton* compressionColorLevelLow;
  QRadioButton* compressionColorLevelVeryLow;
  QCheckBox* compressionCustomCompressionLevel;
  QSpinBox* compressionCustomCompressionLevelTextEdit;
  QCheckBox* compressionJPEGCompression;
  QSpinBox* compressionJPEGCompressionTextEdit;
};

#endif // COMPRESSIONTAB_H
