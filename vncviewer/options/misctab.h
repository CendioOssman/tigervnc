#ifndef MISCTAB_H
#define MISCTAB_H

#include <QWidget>
#include "../optionsdialog.h"

class QCheckBox;

class MiscTab : public TabElement
{
    Q_OBJECT
public:
    MiscTab(QWidget *parent = nullptr);

    void apply();
    void reset();

private:
    QCheckBox* shared;
    QCheckBox* reconnect;
};

#endif // MISCTAB_H
