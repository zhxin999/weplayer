#-------------------------------------------------
#
# Project created by QtCreator 2019-10-07T11:19:58
#
#-------------------------------------------------

QT       += core gui xml network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

RC_ICONS = logo.ico

TARGET = wePlayer
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

win32 {
    LIBS += -L$$PWD/libs
    INCLUDEPATH += $$PWD/util $$PWD/player $$PWD/includes
    DEFINES += AV_OS_WIN32 WIN32_LEAN_AND_MEAN
    LIBS += -lavcodec -lwinmm -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale -lws2_32 -luv -lportaudio_x86

    CONFIG(debug, debug|release) {
        #LIBS += -lvld
        LIBS += -lyuv_mtd
        TARGET=wePlayer
    } else {
        LIBS += -lyuv
        TARGET=wePlayer
    }
}
android {
    LIBS += -L$$PWD/libs
    INCLUDEPATH += $$PWD/util $$PWD/player $$PWD/includes
    #DEFINES += AV_OS_WIN32
    LIBS += -lavcodec -lwinmm -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale -lportaudio_x86

    CONFIG(debug, debug|release) {
        TARGET=wePlayer
    } else {
        TARGET=wePlayer
    }
}

unix{
    DEFINES += _UOS_X86_64
    contains(DEFINES, _UOS_X86_64){
        LIBS += -L$$PWD/UOS/lib
        INCLUDEPATH += $$PWD/util $$PWD/player $$PWD/UOS/include
        #DEFINES += AV_OS_WIN32
        LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale -luv_a -ldl -lportaudio -lasound -lyuv

        CONFIG(debug, debug|release) {
            TARGET=wePlayer_d
        } else {
            TARGET=wePlayer
        }
    } else {
        LIBS += -L$$PWD/libs
        INCLUDEPATH += $$PWD/util $$PWD/player $$PWD/includes
        #DEFINES += AV_OS_WIN32
        LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale -luv_a -ldl -lportaudio

        CONFIG(debug, debug|release) {
            TARGET=wePlayer_d
        } else {
            TARGET=wePlayer
        }
    }

}


SOURCES += \
        main.cpp \
        mainwindow.cpp \
    QWePlayer.cpp \
    CWeListWidget.cpp \
    CWeSlider.cpp \
    CWePlayerCfg.cpp \
    player/anPlayer.cpp \
    util/anListObject.cpp \
    util/anLogs.cpp \
    util/anMisc.cpp \
    util/anRingQueneLF.cpp \
    CWeVideoRender.cpp \
    CAudioDevs.cpp \
    CWeWidgetMisc.cpp \
    dlgoption.cpp \
    WePlayerSkin.cpp \
    WePlayerLayout.cpp \
    DlgInFile.cpp

HEADERS += \
        mainwindow.h \
    QWePlayer.h \
    CWeListWidget.h \
    CWeSlider.h \
    CWeWidgetMisc.h \
    CWePlayerCfg.h \
    anIncludes.h \
    player/anPlayer.h \
    util/anListObject.h \
    util/anListObject_def.h \
    util/anLogs.h \
    util/anMisc.h \
    util/anRingQueneLF.h \
    CWeVideoRender.h \
    CAudioDevs.h \
    dlgoption.h \
    WePlayerSkin.h \
    WePlayerLayout.h \
    DlgInFile.h

FORMS += \
        mainwindow.ui \
    QWePlayer.ui \
    dlgoption.ui \
    DlgInFile.ui

RESOURCES += \
    resource.qrc
