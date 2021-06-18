QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/data/forbid.cpp \
    src/data/railstation.cpp \
    src/data/railinterval.cpp \
    src/data/railway.cpp \
    src/data/ruler.cpp \
    src/data/rulernode.cpp \
    src/data/stationname.cpp \
    src/main.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/data/forbid.h \
    src/data/railintervaldata.hpp \
    src/data/railintervalnode.hpp \
    src/data/railstation.h \
    src/data/railinterval.h \
    src/data/railway.h \
    src/data/ruler.h \
    src/data/rulernode.h \
    src/data/stationname.h \
    src/mainwindow.h \
    src/util/qeexceptions.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
