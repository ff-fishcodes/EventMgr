#ifndef BACKEND_BRIDGE_H
#define BACKEND_BRIDGE_H

#include <QObject>
#include <QVector>
#include <QString>
#include "../backend/event_types.h"
#include "../backend/config_manager.h"
#include "../backend/linkage_engine.h"
#include "../backend/event_manager.h"
#include "../backend/external_api.h"

// ============================================================
// BackendBridge — Qt 与纯 C++ 后端的桥接层
// 一体模式下直接持有后端实例，分离模式下可替换为 Socket 代理
// ============================================================
class BackendBridge : public QObject {
    Q_OBJECT
public:
    explicit BackendBridge(QObject* parent = nullptr);
    ~BackendBridge();

    // 初始化后端模块 + 注册 handler + 配置事件联动
    void initialize();

    ExternalAPI& api() { return *api_; }

    // observe 对接
    void triggerAlarm(const QString& id, bool isActive);

    // 获取活跃事件
    struct EventEntry {
        QString id;
        QString description;
        QString timestamp;
        int     level;
        bool    shielded;
    };
    QVector<EventEntry> getActiveEvents() const;

    // 查询报警目录
    struct CatalogEntry {
        QString id;
        QString description;
        int     originalLevel;
        bool    downgraded;
        int     downgradeTo;
        bool    shielded;
    };
    QVector<CatalogEntry> getCatalog() const;

    // 配置操作
    void setDowngrade(const QString& id, int newLevel);
    void removeDowngrade(const QString& id);
    void setShield(const QString& id);
    void clearShield(const QString& id);
    int  shieldCount() const;

signals:
    // 后端事件产生/消除时触发，前端控件连接此信号实现即时刷新
    void eventsChanged();

    // LinkageEngine 中未注册的动作通过此信号传递给宿主项目
    // 宿主连接此信号自行处理 UI 锁控等动作
    // name 格式如 "lock_ui:panel_main"、"unlock_ui:panel_main"
    void linkageAction(const QString& name);

private:
    ConfigManager*  configMgr_;
    LinkageEngine*  linkageEng_;
    EventManager*   eventMgr_;
    ExternalAPI*    api_;
};

#endif
