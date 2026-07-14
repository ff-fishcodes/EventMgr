# ============================================================
# 事件管理中心 — 前端 Qt 5.15.10 工程
# 编译: qmake && make
# ============================================================

QT       += core gui widgets concurrent
TEMPLATE = app
CONFIG  += c++11
TARGET   = EventMgrFrontend

FORMS += \
    event_list_widget.ui \
    alarm_catalog_widget.ui \
    alarm_log_widget.ui

SOURCES += \
    main.cpp \
    eventmgr_widget.cpp \
    alarm_catalog_widget.cpp \
    event_list_widget.cpp \
    alarm_log_widget.cpp \
    backend_bridge.cpp \
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
    eventmgr_widget.h \
    alarm_catalog_widget.h \
    event_list_widget.h \
    alarm_log_widget.h \
    backend_bridge.h

INCLUDEPATH += .. ../backend
