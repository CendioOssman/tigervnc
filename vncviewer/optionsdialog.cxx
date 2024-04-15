#include "optionsdialog.h"

#include "appmanager.h"
#include "options/compressiontab.h"
#include "options/displaytab.h"
#include "options/inputtab.h"
#include "options/misctab.h"
#include "options/securitytab.h"
#include "i18n.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QProxyStyle>

#ifdef Q_OS_LINUX
class ListViewStyle : public QProxyStyle
{
public:
  void drawPrimitive(PrimitiveElement element, const QStyleOption * option, QPainter * painter, const QWidget * widget = 0) const
  {
    if (PE_FrameFocusRect == element) {
      return;
    }

    if (PE_PanelItemViewItem == element) {
      auto newOption = *qstyleoption_cast<const QStyleOptionViewItem *>(option);
      newOption.showDecorationSelected = true;
      QProxyStyle::drawPrimitive(element, &newOption, painter, widget);
      return;
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
  }
};
#endif

class OptionsDelegate : public QStyledItemDelegate
{
public:
  explicit OptionsDelegate(QObject *parent = nullptr)
    : QStyledItemDelegate(parent)
  {

  }

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
  {
    int width = option.fontMetrics.boundingRect(index.data().toString()).width();
    return QSize(width + 40, 40);
  }
};

OptionsDialog::OptionsDialog(bool staysOnTop, QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(_("VNC Viewer: Connection Options"));
  setWindowFlag(Qt::WindowStaysOnTopHint, staysOnTop);
  setModal(true);
  setWindowModality(Qt::ApplicationModal);


  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);

  QHBoxLayout* hLayout = new QHBoxLayout;

  QListWidget* listWidget = new QListWidget;
#ifdef Q_OS_LINUX
  listWidget->setStyle(new ListViewStyle);
#endif
  listWidget->setFrameShape(QFrame::NoFrame);
  listWidget->setItemDelegate(new OptionsDelegate(this));
  QStringList tabs = {"   " + QString(_("Compression")),
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
                      "   " + QString(_("Security")),
#endif
                      "   " + QString(_("Input")),
                      "   " + QString(_("Display")),
                      "   " + QString(_("Misc"))};
  listWidget->addItems(tabs);
  listWidget->setCurrentRow(0);
  listWidget->setFixedWidth(listWidget->sizeHintForColumn(0));

  hLayout->addWidget(listWidget);

  QFrame* vFrame = new QFrame;
  vFrame->setFrameShape(QFrame::StyledPanel);
  vFrame->setFixedWidth(1);
  hLayout->addWidget(vFrame);

  tabWidget = new QStackedWidget;
  tabWidget->addWidget(new CompressionTab);
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
  tabWidget->addWidget(new SecurityTab);
#endif
  tabWidget->addWidget(new InputTab);
  tabWidget->addWidget(new DisplayTab);
  tabWidget->addWidget(new MiscTab);

  hLayout->addWidget(tabWidget, 1);

  connect(listWidget, &QListWidget::currentRowChanged, tabWidget, &QStackedWidget::setCurrentIndex);

  layout->addLayout(hLayout);

  QFrame* hFrame = new QFrame;
  hFrame->setFrameShape(QFrame::StyledPanel);
  hFrame->setFixedHeight(1);
  layout->addWidget(hFrame);

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->setContentsMargins(10,10,10,10);
  btnsLayout->addStretch(1);
  btnsLayout->setSpacing(5);
  QPushButton* applyBtn = new QPushButton(_("Apply"));
  btnsLayout->addWidget(applyBtn, 0, Qt::AlignRight);
  QPushButton* closeBtn = new QPushButton(_("Close"));
  btnsLayout->addWidget(closeBtn, 0, Qt::AlignRight);
  layout->addLayout(btnsLayout);

  setLayout(layout);

  setMinimumSize(600, 600);

  reset();

  connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
  connect(applyBtn, &QPushButton::clicked, this, &OptionsDialog::apply);
}

void OptionsDialog::apply()
{
  for (int i = 0; i < tabWidget->count(); ++i) {
    auto w = qobject_cast<TabElement*>(tabWidget->widget(i));
    if (w) {
      w->apply();
    }
  }
  AppManager::instance()->handleOptions();
  close();
}

void OptionsDialog::reset()
{
  for (int i = 0; i < tabWidget->count(); ++i) {
    auto w = qobject_cast<TabElement*>(tabWidget->widget(i));
    if (w) {
      w->reset();
    }
  }
}
