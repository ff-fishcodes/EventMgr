#ifndef LINKAGE_ENGINE_H
#define LINKAGE_ENGINE_H

#include "event_types.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>

// ============================================================
// LinkageEngine — 联动引擎（命名回调制）
//
// ActionRegistry 在启动时注册"名字→回调"映射。
// configureEvent / setLevelDefault 用名字字符串引用动作。
//
// 名字格式: "name"          → 默认为 event.protocolID:name
//           "2:name"        → 显式指定 protocolID=2（跨设备）
//           "lock_ui:xxx"   → 前缀=0 为全局动作
//
// 运行时链路:
//   addEvent → 查事件配置表 + 等级默认表 → 名字列表
//   → resolveName(名字, event.protocolID) → 查 actionTable_
//   → 执行回调（lambda 已捕获 this+参数）
// ============================================================
class LinkageEngine {
public:
    // 回调: 无参数 — 所有参数已在 lambda 中捕获
    using ActionCallback = std::function<void()>;

    // ========= Action 注册 =========

    // 注册一个命名回调
    // protocolID=0 为全局（lock_ui 等），>0 为指定下位机
    void registerAction(int protocolID, const std::string& name,
                        ActionCallback callback);

    // ========= 事件联动配置 =========

    // 为指定 EventId 配置联动（名字列表）
    void configureEvent(const EventId& eventId,
                        const std::vector<std::string>& activeNames,
                        const std::vector<std::string>& clearNames);

    // 按等级设置默认联动
    void setLevelDefault(EventLevel level,
                         const std::vector<std::string>& names);

    // ========= 执行 =========

    void executeActive(const Event& event);
    void executeCleared(const Event& event);

    void clearAll();

private:
    // 解析: "name" → ("protocolID", "name")
    //       "2:name" → ("2", "name")
    static std::pair<int, std::string> parseName(const Event& event,
                                                  const std::string& name);

    // 合并：事件配置 + 等级默认 → 名字列表
    std::vector<std::string> resolveActiveNames(const Event& event);

    // 合并：事件配置的 clearNames + LockUI 自动 mirror
    std::vector<std::string> resolveClearNames(const Event& event);

    // 按名字列表执行
    void executeNames(const Event& event, const std::vector<std::string>& names);

    // ========= 存储 =========

    // (protocolID, name) → callback
    std::unordered_map<std::string, ActionCallback> actionTable_;

    // EventId → (activeNames, clearNames)
    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > > eventConfig_;

    // level → default names
    std::unordered_map<int, std::vector<std::string> > levelDefaults_;
};

#endif // LINKAGE_ENGINE_H
