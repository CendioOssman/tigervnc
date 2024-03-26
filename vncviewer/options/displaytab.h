#ifndef DISPLAYTAB_H
#define DISPLAYTAB_H

#include <QWidget>
#include "../optionsdialog.h"

class ScreensSelectionWidget;
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
    ScreensSelectionWidget* selectedScreens;
};

#endif // DISPLAYTAB_H
