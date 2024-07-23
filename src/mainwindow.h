#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QLabel>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

#include "qtermwidget.h"
#include "ptyqt.h"
#include "QGoodWindow"
#include "QGoodCentralWidget"
#include "statusbarwidget.h"
#include "notifictionwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class CentralWidget;
}
QT_END_NAMESPACE

class CentralWidget : public QMainWindow
{
    Q_OBJECT

public:
    CentralWidget(QWidget *parent = nullptr);
    ~CentralWidget();
    void checkCloseEvent(QCloseEvent *event);

protected:
    void contextMenuEvent(QContextMenuEvent *event);

private:
    Ui::CentralWidget *ui;
    QTermWidget *term;
    IPtyProcess *localShell;
    QLabel *statusBarMessage;
    StatusBarWidget *statusBarWidget;
    QWidget *sideProxyWidget;
    QPushButton *sidePushButton;
    QHBoxLayout *sideHboxLayout;
    NotifictionWidget *notifictionWidget;
};  


class MainWindow : public QGoodWindow
{
    Q_OBJECT
public:
    explicit MainWindow(bool isDark = true, QWidget *parent = nullptr);
    ~MainWindow();
    void setLaboratoryButton(QToolButton *laboratoryButton) {
        QTimer::singleShot(0, this, [this, laboratoryButton](){
            laboratoryButton->setFixedSize(m_good_central_widget->titleBarHeight(),m_good_central_widget->titleBarHeight());
            m_good_central_widget->setRightTitleBarWidget(laboratoryButton, false);
            connect(m_good_central_widget,&QGoodCentralWidget::windowActiveChanged,this, [laboratoryButton](bool active){
                laboratoryButton->setEnabled(active);
            });
        });
    }
    void fixMenuBarWidth(void) {
        if (m_menu_bar) {
            /* FIXME: Fix the width of the menu bar 
             * please optimize this code */
            int width = 0;
            int itemSpacingPx = m_menu_bar->style()->pixelMetric(QStyle::PM_MenuBarItemSpacing);
            for (int i = 0; i < m_menu_bar->actions().size(); i++) {
                QString text = m_menu_bar->actions().at(i)->text();
                QFontMetrics fm(m_menu_bar->font());
                width += fm.size(0, text).width() + itemSpacingPx*1.5;
            }
            m_good_central_widget->setLeftTitleBarWidth(width);
        }
    }

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QGoodCentralWidget *m_good_central_widget;
    QMenuBar *m_menu_bar = nullptr;
    CentralWidget *m_central_widget;
};

#endif // MAINWINDOW_H
