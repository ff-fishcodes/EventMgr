#include "backend_bridge.h"
#include "../backend/setup.h"
#include "../backend/device_controllers_example.h"

BackendBridge::BackendBridge(QObject* parent) : QObject(parent),
    configMgr_(nullptr), linkageEng_(nullptr), eventMgr_(nullptr), api_(nullptr) {}

BackendBridge::~BackendBridge() {
    delete api_;
    delete eventMgr_;
    delete linkageEng_;
    delete configMgr_;
}

void BackendBridge::initialize() {
    // 创建后端模块
    configMgr_  = new ConfigManager();
    linkageEng_ = new LinkageEngine();
    eventMgr_   = new EventManager(*configMgr_, *linkageEng_);
    api_        = new ExternalAPI(*eventMgr_, *configMgr_);

    // 注册通用 handler（LockUI/UnlockUI/Buzzer）
    registerLinkageHandlers(*linkageEng_);

    // 各设备单例注册 SendCommand handler
    BoilerController::instance().registerWith(*linkageEng_);
    CoolingTowerController::instance().registerWith(*linkageEng_);

    // 预配置事件联动
    {
        std::vector<LinkageAction> active, clear;
        active.push_back(LinkageAction(LinkageAction::SendCommand, "emergency_stop", "99", 1));
        active.push_back(LinkageAction(LinkageAction::SendCommand, "set_fan_speed", "1200", 2));
        active.push_back(LinkageAction(LinkageAction::LockUI, "panel_main", "", 0));
        linkageEng_->configureEvent("1-3-temp_high", active, clear);
    }
    {
        std::vector<LinkageAction> active, clear;
        active.push_back(LinkageAction(LinkageAction::SendCommand, "emergency_stop", "immediate", 2));
        linkageEng_->configureEvent("2-1-vibration", active, clear);
    }
    {
        std::vector<LinkageAction> active, clear;
        active.push_back(LinkageAction(LinkageAction::LockUI, "panel_device4", "", 0));
        linkageEng_->configureEvent("4-0-device_offline", active, clear);
    }
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
