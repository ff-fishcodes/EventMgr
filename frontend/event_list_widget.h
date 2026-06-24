#ifndef EVENT_LIST_WIDGET_H
#define EVENT_LIST_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QTimer>
#include "backend_bridge.h"

// ============================================================
// EventListWidget — 事件列表页（Tab 1）
// 显示当前活跃告警，按等级着色，支持手动清除
// ============================================================
class EventListWidget : public QWidget {
    Q_OBJECT
public:
    explicit EventListWidget(BackendBridge* bridge, QWidget* parent = nullptr);

    // 模拟收到报警（实际由 Socket 推送触发，此处用按钮模拟）
    void simulateAddEvent(const QString& id, int protocolID, int frameID,
                          const QString& alarmField, int level,
                          const QString& desc);

public slots:
    // 刷新列表
    void refresh();

private slots:
    void onSimulateBtn();

private:
    void setupUI();
    void addRow(const BackendBridge::CatalogEntry& entry);

    BackendBridge* bridge_;
    QTableWidget*  table_;
    QLabel*        statusLabel_;
};

#endif
