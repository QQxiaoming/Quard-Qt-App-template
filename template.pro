QT       += core gui multimedia
CONFIG += c++17

include(./lib/qtermwidget/qtermwidget.pri)
include(./lib/QFontIcon/QFontIcon.pri)
include(./lib/QSourceHighlite/QSourceHighlite.pri)
include(./lib/ptyqt/ptyqt.pri)
include(./lib/QGoodWindow/QGoodWindow/QGoodWindow.pri)
include(./lib/QGoodWindow/QGoodCentralWidget/QGoodCentralWidget.pri)

INCLUDEPATH += \
    src/util \
    src

SOURCES += \
    src/util/misc.cpp \
    src/util/globalsetting.cpp \
    src/util/filedialog.cpp \
    src/statusbarwidget.cpp \
    src/notificationitem.cpp \
    src/notifictionwidget.cpp \
    src/mainwindow.cpp \
    src/main.cpp

HEADERS += \
    src/util/misc.h \
    src/util/globalsetting.h \
    src/util/filedialog.h \
    src/statusbarwidget.h \
    src/notificationitem.h \
    src/notifictionwidget.h \
    src/mainwindow.h

FORMS += \
    src/util/filedialog.ui \
    src/statusbarwidget.ui \
    src/notificationitem.ui \
    src/notifictionwidget.ui \
    src/mainwindow.ui

RESOURCES += \
    res/resource.qrc

win32:{
    win32-g++ {
        QMAKE_CXXFLAGS += -Wno-deprecated-copy
        RESOURCES += res/terminalfont.qrc
    }
    win32-msvc*{
    }

    VERSION = 1.0.0.000
    RC_ICONS = "icons\ico.ico"
    QMAKE_TARGET_PRODUCT = "template"
    QMAKE_TARGET_DESCRIPTION = "template based on Qt $$[QT_VERSION]"
    QMAKE_TARGET_COPYRIGHT = "GNU General Public License v3.0"
}

unix:!macx:{
    RESOURCES += res/terminalfont.qrc
    QMAKE_CXXFLAGS += -Wno-deprecated-copy
    QMAKE_RPATHDIR=$ORIGIN
    QMAKE_LFLAGS += -no-pie 
    LIBS += -lutil
}

macx:{
    RESOURCES += res/terminalfont.qrc
    QMAKE_CXXFLAGS += -Wno-deprecated-copy
    QMAKE_RPATHDIR=$ORIGIN
    ICON = "icons\ico.icns"
    LIBS += -lutil
}
