#ifndef BACKEND_BRIDGE_H
#define BACKEND_BRIDGE_H

#include <QObject>
#include <QVector>
#include <QString>
#include "../backend/event_types.h"

// ============================================================
// BackendBridge — Qt 桥接层
// 一体模式下通过单例访问后端，分离模式下可替换为 Socket 代理
// ============================================================
class BackendBridge : public QObject {
    Q_OBJECT
public:
    explicit BackendBridge(QObject* parent = nullptr);
    ~BackendBridge();

    // 初始化后端模块 + 注入回调
    void initialize();

    // observe 对接
    void triggerAlarm(const QString& id, bool isActive);

    // 获取活跃事件
    struct EventEntry {
        QString id;
        QString description;
        QString timestamp;
        int     level;
        bool    downgraded;
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

    // 联动禁用（isActive=true 产生侧, false 消除侧）
    void disableAction(const QString& eventId, const QString& actionName, bool isActive);
    void enableAction(const QString& eventId, const QString& actionName, bool isActive);

    // 查询事件联动能力及禁用状态
    struct ActionEntry {
        QString name;
        QString displayName;
        bool    disabledActive;
        bool    disabledClear;
    };
    QVector<ActionEntry> getEventActions(const QString& eventId) const;

signals:
    void eventsChanged();
    void linkageAction(const QString& eventId, bool isActive);
};

#endif
