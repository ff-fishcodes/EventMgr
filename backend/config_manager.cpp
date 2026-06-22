#include "config_manager.h"

// ========= 降级相关 =========

void ConfigManager::setDowngrade(const EventId& id, EventLevel newLevel) {
    if (newLevel < EventLevel::Emergency || newLevel > EventLevel::Info) {
        return; // 无效等级，忽略
    }
    downgradeMap_[id] = newLevel;
}

void ConfigManager::removeDowngrade(const EventId& id) {
    downgradeMap_.erase(id);
}

EventLevel ConfigManager::getEffectiveLevel(const EventId& id,
                                             EventLevel originalLevel) const {
    auto it = downgradeMap_.find(id);
    if (it != downgradeMap_.end()) {
        return it->second;   // 返回降级后的等级
    }
    return originalLevel;    // 无降级配置，保持原等级
}

bool ConfigManager::hasDowngrade(const EventId& id) const {
    return downgradeMap_.find(id) != downgradeMap_.end();
}

// ========= 屏蔽相关 =========

void ConfigManager::setShield(const EventId& id) {
    shieldSet_.insert(id);
}

void ConfigManager::clearShield(const EventId& id) {
    shieldSet_.erase(id);
}

bool ConfigManager::isShielded(const EventId& id) const {
    return shieldSet_.find(id) != shieldSet_.end();
}

int ConfigManager::getShieldCount() const {
    return static_cast<int>(shieldSet_.size());
}

// ========= 批量操作 =========

void ConfigManager::clearAll() {
    downgradeMap_.clear();
    shieldSet_.clear();
}
