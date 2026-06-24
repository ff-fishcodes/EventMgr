#ifndef ALARM_CATALOG_WIDGET_H
#define ALARM_CATALOG_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include "backend_bridge.h"

// ============================================================
// AlarmCatalogWidget — 报警配置页（Tab 2）
// 显示全量报警定义，支持降级/屏蔽的事前配置
// ============================================================
class AlarmCatalogWidget : public QWidget {
    Q_OBJECT
public:
    explicit AlarmCatalogWidget(BackendBridge* bridge, QWidget* parent = nullptr);

public slots:
    void loadCatalog();     // 加载/刷新报警目录
    void onApply();         // 应用配置

private:
    void setupUI();

    BackendBridge* bridge_;
    QTableWidget*  table_;
    QLabel*        statusLabel_;
};

#endif
