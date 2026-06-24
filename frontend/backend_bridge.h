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

    // 后端接口代理
    ExternalAPI& api() { return *api_; }

    // 查询报警目录（转为 Qt 友好格式）
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

private:
    ConfigManager*  configMgr_;
    LinkageEngine*  linkageEng_;
    EventManager*   eventMgr_;
    ExternalAPI*    api_;
};

#endif
