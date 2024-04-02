#ifndef CONTEXTMENUACTIONS_H
#define CONTEXTMENUACTIONS_H

#include <QAction>

class QMenuSeparator : public QAction
{
public:
  QMenuSeparator(QWidget* parent = nullptr);
};

class QCheckableAction : public QAction
{
public:
  QCheckableAction(const QString& text, QWidget* parent = nullptr);
};

class QFullScreenAction : public QCheckableAction
{
public:
  QFullScreenAction(const QString& text, QWidget* parent = nullptr);
};

class QRevertSizeAction : public QAction
{
public:
  QRevertSizeAction(const QString& text, QWidget* parent = nullptr);
};

class QKeyToggleAction : public QCheckableAction
{
public:
  QKeyToggleAction(const QString& text, int keyCode, quint32 keySym, QWidget* parent = nullptr);

private:
  int keyCode;
  quint32 keySym;
};

class QMenuKeyAction : public QAction
{
public:
  QMenuKeyAction(QWidget* parent = nullptr);
};

class QCtrlAltDelAction : public QAction
{
public:
  QCtrlAltDelAction(const QString& text, QWidget* parent = nullptr);
};

class QMinimizeAction : public QAction
{
public:
  QMinimizeAction(const QString& text, QWidget* parent = nullptr);
};

class QDisconnectAction : public QAction
{
public:
  QDisconnectAction(const QString& text, QWidget* parent = nullptr);
};

class QOptionDialogAction : public QAction
{
public:
  QOptionDialogAction(const QString& text, QWidget* parent = nullptr);
};

class QRefreshAction : public QAction
{
public:
  QRefreshAction(const QString& text, QWidget* parent = nullptr);
};

class QInfoDialogAction : public QAction
{
public:
  QInfoDialogAction(const QString& text, QWidget* parent = nullptr);
};

class QAboutDialogAction : public QAction
{
public:
  QAboutDialogAction(const QString& text, QWidget* parent = nullptr);
};

#endif // CONTEXTMENUACTIONS_H
