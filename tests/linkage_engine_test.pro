QT += core testlib
QT -= gui
TEMPLATE = app
CONFIG += console c++11 testcase
TARGET = test_linkage_engine

SOURCES += \
    test_linkage_engine.cpp \
    ../backend/linkage_engine.cpp

HEADERS += \
    ../backend/linkage_engine.h \
    ../backend/event_types.h

INCLUDEPATH += .. ../backend
