#include <QUrl>
#include <QProcess>
#include <QFontDatabase>
#include <QFont>
#include <QApplication>
#include <QMessageBox>
#include <QDesktopServices>

#include "misc.h"
#include "mainwindow.h"
#include "globalsetting.h"
#include "qfonticon.h"
#include "filedialog.h"
#include "ui_mainwindow.h"

CentralWidget::CentralWidget(bool isDark, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CentralWidget)
    , isDarkTheme(isDark)
{
    ui->setupUi(this);

    sidePushButton = new QPushButton("开始");
    sidePushButton->setFixedSize(250,26);
    sideHboxLayout = new QHBoxLayout();
    sideHboxLayout->setContentsMargins(0,0,0,0);
    sideHboxLayout->setSpacing(2);
    sideProxyWidget = new QWidget();
    sideProxyWidget->setLayout(sideHboxLayout);
    sideHboxLayout->addWidget(sidePushButton);
    QGraphicsScene *scene = new QGraphicsScene(this);
    QGraphicsProxyWidget *w = scene->addWidget(sideProxyWidget);
    w->setPos(0,0);
    w->setRotation(-90);
    ui->graphicsView->setScene(scene);
    ui->graphicsView->setFrameStyle(0);
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setFixedSize(30, 251);
    ui->sidewidget->setFixedWidth(30);
    ui->labelSide->setVisible(false);

    connect(sidePushButton, &QPushButton::clicked, this, [&](){
        ui->labelSide->setVisible(!ui->labelSide->isVisible());
    });

    notifictionWidget = new NotifictionWidget(this);
    ui->splitter_2->addWidget(notifictionWidget);
    notifictionWidget->hide();
    ui->splitter_2->setSizes({100, 600, 100});

    term = new QTermWidget(this,this);
    term->setUrlFilterEnabled(false);
    term->setScrollBarPosition(QTermWidget::ScrollBarRight);
    term->setBlinkingCursor(false);
    term->setMargin(0);
    term->setDrawLineChars(false);
    term->setSelectionOpacity(0.5);
    term->setEnableHandleCtrlC(true);
    term->set_fix_quardCRT_issue33(true);

    QFont font = QApplication::font();
#if defined(Q_OS_WIN) && defined(Q_CC_MSVC)
    int fontId = QFontDatabase::addApplicationFont(QApplication::applicationDirPath() + "/inziu-iosevkaCC-SC-regular.ttf");
#else
    int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/font/font/inziu-iosevkaCC-SC-regular.ttf"));
#endif
    QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (fontFamilies.size() > 0) {
        font.setFamily(fontFamilies[0]);
    }
    font.setFixedPitch(true);
    GlobalSetting settings;
#if defined(Q_OS_MACOS)
    int defaultFontsize = 15;
#else
    int defaultFontsize = 12;
#endif
    int fontsize = settings.value("Global/fontsize", defaultFontsize).toInt();
    font.setPointSize(fontsize);
    term->setTerminalFont(font);
    QStringList availableColorSchemes = term->availableColorSchemes();
    availableColorSchemes.sort();
    QString currentColorScheme = availableColorSchemes.first();
    foreach(QString colorScheme, availableColorSchemes) {
        if(colorScheme == "QuardCRT") {
            term->setColorScheme("QuardCRT");
            currentColorScheme = "QuardCRT";
        }
    }
    QStringList availableKeyBindings = term->availableKeyBindings();
    availableKeyBindings.sort();
    QString currentAvailableKeyBindings = availableKeyBindings.first();
    foreach(QString keyBinding, availableKeyBindings) {
        if(keyBinding == "linux") {
            term->setKeyBindings("linux");
            currentAvailableKeyBindings = "linux";
        }
    }
    ui->splitter->addWidget(term);
    ui->splitter->setCollapsible(0, false);
    ui->splitter->setCollapsible(1, false);
    ui->splitter->setSizes({500, 300});

    bool supportSelection = QApplication::clipboard()->supportsSelection();
    if(!supportSelection) {
        connect(term, &QTermWidget::mousePressEventForwarded, this, [&](QMouseEvent *event) {
            if(event->button() == Qt::MiddleButton) {
                term->copyClipboard();
                term->pasteClipboard();
            }
        });
    }
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    connect(term, &QTermWidget::handleCtrlC, this, [&](void){
        QString text = term->selectedText();
        if(!text.isEmpty()) {
            QApplication::clipboard()->setText(text, QClipboard::Clipboard);
        }
    });
#endif

    localShell = PtyQt::createPtyProcess();
    connect(term, &QTermWidget::termSizeChange, this, [=](int lines, int columns){
        localShell->resize(columns,lines);
    });
    QString shellPath;
    QStringList args;
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if(shellPath.isEmpty()) shellPath = qEnvironmentVariable("SHELL");
    if(shellPath.isEmpty()) shellPath = "/bin/sh";
#elif defined(Q_OS_WIN)
    if(shellPath.isEmpty()) shellPath = "c:\\Windows\\system32\\WindowsPowerShell\\v1.0\\powershell.exe";
    args =  {
        "-ExecutionPolicy",
        "Bypass",
        "-NoLogo",
        "-NoProfile",
        "-NoExit",
        "-File",
        "\"" + QApplication::applicationDirPath() + "/Profile.ps1\""
    };
#endif
    QStringList envs = QProcessEnvironment::systemEnvironment().toStringList();
#if defined(Q_OS_MACOS)
    bool defaultForceUTF8 = true;
#else
    bool defaultForceUTF8 = false;
#endif
    if(defaultForceUTF8) {
        envs.append("LC_CTYPE=UTF-8");
    }
    QString workingDirectory = QDir::homePath();
    QFileInfo fileInfo(workingDirectory);
    if(!fileInfo.exists() || !fileInfo.isDir()) {
        workingDirectory = QDir::homePath();
    }
    bool ret = localShell->startProcess(shellPath, args, workingDirectory, envs, term->screenColumnsCount(), term->screenLinesCount());
    if(!ret) {
        QMessageBox::warning(this, "启动Shell", QString("无法启动本地Shell:\n%1.").arg(localShell->lastError()));
    }
    connect(localShell->notifier(), &QIODevice::readyRead, this, [=](){
        QByteArray data = localShell->readAll();
        term->recvData(data.data(), data.size());
    });
    connect(term, &QTermWidget::sendData, this, [=](const char *data, int size){
        localShell->write(QByteArray(data, size));
    });

    statusBarWidget = new StatusBarWidget(ui->statusBar);
    ui->statusBar->addPermanentWidget(statusBarWidget,0);
    ui->statusBar->setSizeGripEnabled(false);

    connect(notifictionWidget,&NotifictionWidget::notifictionChanged,this,[&](uint32_t count){
        statusBarWidget->setNotifiction(count?true:false);
    });
    connect(statusBarWidget->statusBarNotifiction,&StatusBarToolButton::clicked,this,[&](){
        if(notifictionWidget->isHidden()) {
            notifictionWidget->show();
        } else {
            notifictionWidget->hide();
        }
    });

    QTimer::singleShot(0, this, [this,parent](){
        MainWindow *mainWindow = static_cast<MainWindow*>(parent);
        //mainWindow->fixMenuBarWidth();
    #if defined(Q_OS_LINUX)
        // FIXME: only for linux, this is a bad hack, but it works
        connect(mainWindow,&QGoodWindow::fixIssueWindowEvent,this,[this](QEvent::Type type){
            if(type == QEvent::Resize) {
                term->repaintDisplay();
            }
        });
    #else
        Q_UNUSED(mainWindow);
    #endif
    });
}

CentralWidget::~CentralWidget()
{
    if(localShell) {
        localShell->kill();
        delete localShell;
    }
    delete term;
    delete ui;
}

void CentralWidget::contextMenuEvent(QContextMenuEvent *event) {
    if (!term->geometry().contains(event->pos())) {
        return;
    }

    QMenu *menu = new QMenu(this);
    QList<QAction*> ftActions = term->filterActions(event->pos());
    if(!ftActions.isEmpty()) {
        menu->addActions(ftActions);
        menu->addSeparator();
    }
    QAction *copyAction = new QAction("复制",this);
    connect(copyAction,&QAction::triggered,this,[&](){
        term->copyClipboard();
    });
    menu->addAction(copyAction);

    QAction *pasteAction = new QAction("粘贴",this);
    connect(pasteAction,&QAction::triggered,this,[&](){
        term->pasteClipboard();
    });
    menu->addAction(pasteAction);

    QAction *selectAllAction = new QAction("全选",this);
    connect(selectAllAction,&QAction::triggered,this,[&](){
        term->selectAll();
    });
    menu->addAction(selectAllAction);
    
    QAction *findAction = new QAction("查找",this);
    connect(findAction,&QAction::triggered,this,[&](){
        term->toggleShowSearchBar();
    });
    menu->addAction(findAction);
    menu->addSeparator();

    QAction *zoomInAction = new QAction("放大",this);
    connect(zoomInAction,&QAction::triggered,this,[&](){
        term->zoomIn();
    });
    menu->addAction(zoomInAction);

    QAction *zoomOutAction = new QAction("缩小",this);
    connect(zoomOutAction,&QAction::triggered,this,[&](){
        term->zoomOut();
    });
    menu->addAction(zoomOutAction);

    QRect screenGeometry = QGuiApplication::screenAt(cursor().pos())->geometry();
    QPoint pos = cursor().pos() + QPoint(5,5);
    if (pos.x() + menu->width() > screenGeometry.right()) {
        pos.setX(screenGeometry.right() - menu->width());
    }
    if (pos.y() + menu->height() > screenGeometry.bottom()) {
        pos.setY(screenGeometry.bottom() - menu->height());
    }
    menu->popup(pos);
}

void CentralWidget::checkCloseEvent(QCloseEvent *event) {
    QMessageBox::StandardButton ret = QMessageBox::question(this, "退出", "确定要退出吗？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if(ret == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}

void CentralWidget::on_lightThemeAction_triggered()
{
    if(!isDarkTheme) return;
    isDarkTheme = false;
    qGoodStateHolder->setCurrentThemeDark(isDarkTheme);
    if(themeColorEnable) QGoodWindow::setAppCustomTheme(isDarkTheme,themeColor);
    QFontIcon::instance()->setColor(Qt::black);
    term->setColorScheme("QuardCRT Light");
    statusBarWidget->retranslateUi();
}

void CentralWidget::on_darkThemeAction_triggered()
{
    if(isDarkTheme) return;
    isDarkTheme = true;
    qGoodStateHolder->setCurrentThemeDark(isDarkTheme);
    if(themeColorEnable) QGoodWindow::setAppCustomTheme(isDarkTheme,themeColor);
    QFontIcon::instance()->setColor(Qt::white);
    term->setColorScheme("QuardCRT");
    statusBarWidget->retranslateUi();
}

void CentralWidget::on_themeColorAction_triggered()
{
    QColor color = QColorDialog::getColor(themeColor, this, tr("Select color"));
    if (color.isValid()) {
        themeColor = color;
        themeColorEnable = true;
        qGoodStateHolder->setCurrentThemeDark(isDarkTheme);
        QGoodWindow::setAppCustomTheme(isDarkTheme,themeColor);
    } else {
        themeColorEnable = false;
        qGoodStateHolder->setCurrentThemeDark(isDarkTheme);
    }
}

MainWindow::MainWindow(bool isDark, QWidget *parent)
    : QGoodWindow(parent) {
    m_central_widget = new CentralWidget(isDark,this);
    m_central_widget->setWindowFlags(Qt::Widget);

    m_good_central_widget = new QGoodCentralWidget(this);

#ifdef Q_OS_MAC
    //macOS uses global menu bar
    if(QApplication::testAttribute(Qt::AA_DontUseNativeMenuBar)) {
#else
    if(true) {
#endif
        m_menu_bar = m_central_widget->menuBar();

        //Set font of menu bar
        QFont font = m_menu_bar->font();
    #ifdef Q_OS_WIN
        font.setFamily("Segoe UI");
    #else
        font.setFamily(qApp->font().family());
    #endif
        m_menu_bar->setFont(font);

        QTimer::singleShot(0, this, [&]{
            const int title_bar_height = m_good_central_widget->titleBarHeight();
            m_menu_bar->setStyleSheet(QString("QMenuBar {height: %0px;}").arg(title_bar_height));
        });

        connect(m_good_central_widget,&QGoodCentralWidget::windowActiveChanged,this, [&](bool active){
            m_menu_bar->setEnabled(active);
        #ifdef Q_OS_MACOS
            fixWhenShowQuardCRTTabPreviewIssue();
        #endif
        });

        m_good_central_widget->setLeftTitleBarWidget(m_menu_bar);
        setNativeCaptionButtonsVisibleOnMac(false);
    } else {
        setNativeCaptionButtonsVisibleOnMac(true);
    }

    connect(qGoodStateHolder, &QGoodStateHolder::currentThemeChanged, this, [](){
        if (qGoodStateHolder->isCurrentThemeDark())
            QGoodWindow::setAppDarkTheme();
        else
            QGoodWindow::setAppLightTheme();
    });
    connect(this, &QGoodWindow::systemThemeChanged, this, [&]{
        qGoodStateHolder->setCurrentThemeDark(QGoodWindow::isSystemThemeDark());
    });
    qGoodStateHolder->setCurrentThemeDark(isDark);

    m_good_central_widget->setCentralWidget(m_central_widget);
    setCentralWidget(m_good_central_widget);

    setWindowIcon(QIcon(":/icons/icons/ico.svg"));
    setWindowTitle(m_central_widget->windowTitle());

    m_good_central_widget->setTitleAlignment(Qt::AlignCenter);

    resize(1200,600);
}

MainWindow::~MainWindow() {
    delete m_central_widget;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_central_widget->checkCloseEvent(event);
}
