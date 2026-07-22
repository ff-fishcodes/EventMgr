#ifndef EVENTMGR_WIDGET_H
#define EVENTMGR_WIDGET_H

#include <QWidget>

class BackendBridge;
class EventListWidget;
class AlarmCatalogWidget;
class QTabWidget;
class QLabel;
class QTimer;

// ============================================================
// EventMgrWidget — 事件管理中心控件
// 可嵌入任意 QMainWindow 或父级 QWidget
// ============================================================
class EventMgrWidget : public QWidget {
    Q_OBJECT
public:
    explicit EventMgrWidget(QWidget* parent = nullptr);
    ~EventMgrWidget();

    // 对外暴露后端接口（供宿主项目使用）
    BackendBridge* backend() const { return bridge_; }

private slots:
    void onTabChanged(int index);
    void updateShieldStatus();

private:
    void setupUI();

    BackendBridge*      bridge_;
    QTabWidget*         tabs_;
    EventListWidget*    eventListPage_;
    AlarmCatalogWidget* catalogPage_;
    QLabel*             shieldLabel_;
    QTimer*             statusTimer_;
    int                 previousTabIndex_;
};

#endif
