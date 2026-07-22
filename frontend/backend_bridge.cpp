#include "backend_bridge.h"
#include "../backend/event_mgr_module.h"
#include "../backend/action_registry.h"
#include "../backend/external_api.h"
#include "../backend/event_manager.h"
#include "../backend/config_manager.h"
#include "../backend/linkage_engine.h"

BackendBridge::BackendBridge(QObject* parent) : QObject(parent) {}

BackendBridge::~BackendBridge() {}

BackendBridge::ActionEntry::ActionEntry()
    : enabled(false) {}

BackendBridge::ActionEntry::ActionEntry(
        const QString& actionName,
        const QString& actionDisplayName,
        bool actionEnabled)
    : name(actionName),
      displayName(actionDisplayName),
      enabled(actionEnabled) {}

BackendBridge::ActionEntry::~ActionEntry() {}

BackendBridge::EventActionGroups::EventActionGroups() {}

BackendBridge::EventActionGroups::~EventActionGroups() {}

void BackendBridge::initialize() {
    // 一键启动后端模块（若已由 EventMgrModule::init() 启动则跳过）
    EventMgrModule::init();

    // 一体模式：注入前端通知回调
    EventManager::instance().setNotifyCallback([this](const std::string&) {
        emit eventsChanged();
    });

    // 一体模式：注入 UI 锁控 fallback
    LinkageEngine::instance().setFallback(
        [this](const std::string& eventId, bool isActive) {
            emit linkageAction(QString::fromStdString(eventId), isActive);
        });
}

void BackendBridge::triggerAlarm(const QString& id, bool isActive) {
    QStringList parts = id.split('-');
    if (parts.size() != 3) return;
    ExternalAPI::instance().triggerAlarm(
        parts[0].toStdString(), parts[1].toInt(),
        parts[2].toStdString(), isActive);
}

QVector<BackendBridge::EventEntry> BackendBridge::getActiveEvents() const {
    QVector<EventEntry> result;
    std::vector<Event> events = ExternalAPI::instance().getActiveEvents();
    for (std::vector<Event>::const_iterator it = events.begin();
         it != events.end(); ++it) {
        EventEntry e;
        e.id          = QString::fromStdString(it->id);
        e.description = QString::fromStdString(it->description);
        e.timestamp   = QString::fromStdString(it->timestamp);
        e.level       = static_cast<int>(it->effectiveLevel);
        e.downgraded  = ConfigManager::instance().hasDowngrade(it->id);
        e.shielded    = ConfigManager::instance().isShielded(it->id);
        result.append(e);
    }
    return result;
}

QVector<BackendBridge::CatalogEntry> BackendBridge::getCatalog() const {
    QVector<CatalogEntry> result;
    std::vector<AlarmDef> defs = ExternalAPI::instance().getAlarmCatalog();
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
    ConfigManager::instance().setDowngrade(
        id.toStdString(), static_cast<EventLevel>(newLevel));
}

void BackendBridge::removeDowngrade(const QString& id) {
    ConfigManager::instance().removeDowngrade(id.toStdString());
}

void BackendBridge::setShield(const QString& id) {
    ConfigManager::instance().setShield(id.toStdString());
}

void BackendBridge::clearShield(const QString& id) {
    ConfigManager::instance().clearShield(id.toStdString());
}

int BackendBridge::shieldCount() const {
    return ConfigManager::instance().getShieldCount();
}

void BackendBridge::disableAction(const QString& eventId,
                                   const QString& actionName, bool isActive) {
    LinkageEngine::instance().disableAction(
        eventId.toStdString(), actionName.toStdString(), isActive);
}

void BackendBridge::enableAction(const QString& eventId,
                                  const QString& actionName, bool isActive) {
    LinkageEngine::instance().enableAction(
        eventId.toStdString(), actionName.toStdString(), isActive);
}

BackendBridge::EventActionGroups BackendBridge::getEventActionGroups(
        const QString& eventId, int originalLevel) const {
    const LinkageEngine::EventActionGroups backendGroups =
        LinkageEngine::instance().getEventActionGroups(
            eventId.toStdString(), static_cast<EventLevel>(originalLevel));
    EventActionGroups result;
    for (std::vector<LinkageEngine::ActionInfo>::const_iterator it =
             backendGroups.activeActions.begin();
         it != backendGroups.activeActions.end(); ++it) {
        result.activeActions.append(ActionEntry(
            QString::fromStdString(it->name),
            QString::fromStdString(it->displayName),
            it->enabled));
    }
    for (std::vector<LinkageEngine::ActionInfo>::const_iterator it =
             backendGroups.clearActions.begin();
         it != backendGroups.clearActions.end(); ++it) {
        result.clearActions.append(ActionEntry(
            QString::fromStdString(it->name),
            QString::fromStdString(it->displayName),
            it->enabled));
    }
    return result;
}
