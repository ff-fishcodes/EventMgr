#include "backend_bridge.h"
#include "../backend/action_registry.h"

BackendBridge::BackendBridge(QObject* parent) : QObject(parent),
    configMgr_(nullptr), linkageEng_(nullptr), eventMgr_(nullptr), api_(nullptr) {}

BackendBridge::~BackendBridge() {
    delete api_;
    delete eventMgr_;
    delete linkageEng_;
    delete configMgr_;
}

void BackendBridge::initialize() {
    configMgr_  = new ConfigManager();
    linkageEng_ = new LinkageEngine();
    eventMgr_   = new EventManager(*configMgr_, *linkageEng_);
    api_        = new ExternalAPI(*eventMgr_, *configMgr_);

    // 集中注册所有能力 + 配置事件联动
    ActionRegistry::setup(*linkageEng_);
}

QVector<BackendBridge::CatalogEntry> BackendBridge::getCatalog() const {
    QVector<CatalogEntry> result;
    std::vector<AlarmDef> defs = api_->getAlarmCatalog();
    for (std::vector<AlarmDef>::const_iterator it = defs.begin();
         it != defs.end(); ++it) {
        CatalogEntry e;
        e.id             = QString::fromStdString(it->id);
        e.description    = QString::fromStdString(it->description);
        e.originalLevel  = static_cast<int>(it->originalLevel);
        e.downgraded     = it->downgraded;
        e.downgradeTo    = static_cast<int>(it->downgradeTo);
        e.shielded       = it->shielded;
        result.append(e);
    }
    return result;
}

void BackendBridge::setDowngrade(const QString& id, int newLevel) {
    configMgr_->setDowngrade(id.toStdString(), static_cast<EventLevel>(newLevel));
}

void BackendBridge::removeDowngrade(const QString& id) {
    configMgr_->removeDowngrade(id.toStdString());
}

void BackendBridge::setShield(const QString& id) {
    configMgr_->setShield(id.toStdString());
}

void BackendBridge::clearShield(const QString& id) {
    configMgr_->clearShield(id.toStdString());
}

int BackendBridge::shieldCount() const {
    return configMgr_->getShieldCount();
}
