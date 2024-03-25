#ifndef DISPLAYTAB_H
#define DISPLAYTAB_H

#include <QWidget>
#include "../optionsdialog.h"

class QRadioButton;

class DisplayTab : public TabElement
{
    Q_OBJECT
public:
    DisplayTab(QWidget *parent = nullptr);

    void apply();
    void reset();

private:
    QRadioButton* displayWindowed;
    QRadioButton* displayFullScreenOnCurrentMonitor;
    QRadioButton* displayFullScreenOnAllMonitors;
    QRadioButton* displayFullScreenOnSelectedMonitors;
};

#endif // DISPLAYTAB_H
