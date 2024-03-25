#ifndef COMPRESSIONTAB_H
#define COMPRESSIONTAB_H

#include <QWidget>
#include "../optionsdialog.h"

class QCheckBox;
class QSpinBox;
class QRadioButton;

class CompressionTab : public TabElement
{
    Q_OBJECT
public:
    CompressionTab(QWidget *parent = nullptr);

    void apply();
    void reset();

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
