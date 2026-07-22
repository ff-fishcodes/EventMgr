QT += core gui widgets testlib concurrent
TEMPLATE = app
CONFIG += console c++11 testcase
TARGET = test_alarm_catalog_widget

FORMS += \
    ../frontend/alarm_catalog_widget.ui \
    ../frontend/event_list_widget.ui

SOURCES += \
    test_alarm_catalog_widget.cpp \
    ../frontend/eventmgr_widget.cpp \
    ../frontend/alarm_catalog_widget.cpp \
    ../frontend/event_list_widget.cpp \
    ../frontend/backend_bridge.cpp \
    ../backend/external_api.cpp \
    ../backend/event_manager.cpp \
    ../backend/linkage_engine.cpp \
    ../backend/config_manager.cpp \
    ../backend/action_registry.cpp \
    ../backend/event_mgr_module.cpp \
    ../backend/system_events.cpp \
    ../backend/stubs/alarm_catalog.cpp \
    ../backend/stubs/socket_server.cpp \
    ../backend/stubs/log_writer.cpp \
    ../backend/stubs/cmd_sender.cpp \
    ../backend/stubs/buzzer_control.cpp

HEADERS += \
    ../frontend/eventmgr_widget.h \
    ../frontend/alarm_catalog_widget.h \
    ../frontend/event_list_widget.h \
    ../frontend/backend_bridge.h

INCLUDEPATH += .. ../backend ../frontend
