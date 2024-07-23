#include <QApplication>

#include "QGoodWindow"
#include "mainwindow.h"
#include "qfonticon.h"

int main(int argc, char *argv[])
{
    QGoodWindow::setup();
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication application(argc, argv);

    bool isDarkTheme = true;
    QFontIcon::addFont(":/icons/icons/fontawesome-webfont.ttf");
    QFontIcon::instance()->setColor(isDarkTheme?Qt::white:Qt::black);

    if(isDarkTheme) {
        QGoodWindow::setAppDarkTheme();
    } else {
        QGoodWindow::setAppLightTheme();
    }

    MainWindow window(isDarkTheme);
    window.show();
    return application.exec();
}
