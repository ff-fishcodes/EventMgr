#ifndef ALARM_LOG_WIDGET_H
#define ALARM_LOG_WIDGET_H

#include <QWidget>
#include <QTimer>
#include "backend_bridge.h"

// UI 生成的头文件（qmake 自动从 .ui 生成）
#include "ui_alarm_log_widget.h"

// ============================================================
// AlarmLogWidget — 实时报警日志控件（可嵌入主界面）
//
// 两列表格：时间 | 报警内容描述
// 每 1 秒轮询 getActiveEvents() 刷新
// 仅显示当前活跃事件，消除后自动消失
// ============================================================
class AlarmLogWidget : public QWidget {
    Q_OBJECT
public:
    explicit AlarmLogWidget(BackendBridge* bridge, QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    Ui::AlarmLogWidget ui;
    BackendBridge* bridge_;
    QTimer*        refreshTimer_;
};

#endif
