#ifndef LINKAGE_ENGINE_H
#define LINKAGE_ENGINE_H

#include "event_types.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

// ============================================================
// LinkageEngine — 联动引擎（命名回调制）
//
// registerAction(name, callback)       — 注册能力
// configureEvent(id, {names}, {names}) — 绑定能力到事件
// setLevelDefault(level, {names})      — 等级默认动作
//
// 运行时: addEvent → resolveNames → 查 actionTable_ → 执行回调
// ============================================================
class LinkageEngine {
public:
    using ActionCallback   = std::function<void()>;
    // 联动通知：executeActive/executeCleared 末尾调用
    // 参数: eventId, isActive（true=产生, false=消除）
    // 宿主项目注入此回调，emit Qt 信号，由 UI 层按事件 ID 自行处理锁控
    using FallbackCallback = std::function<void(const std::string& eventId, bool isActive)>;

    LinkageEngine() {}
    ~LinkageEngine() {}

    // 注册一个命名能力
    void registerAction(const std::string& name, ActionCallback callback);

    // 设置未注册动作的 fallback（只设一个，后来覆盖前者）
    void setFallback(FallbackCallback callback);

    // 为指定事件绑定能力
    void configureEvent(const EventId& eventId,
                        const std::vector<std::string>& activeNames,
                        const std::vector<std::string>& clearNames);

    // 按等级绑定默认能力
    void setLevelDefault(EventLevel level,
                         const std::vector<std::string>& names);

    void executeActive(const Event& event);
    void executeCleared(const Event& event);
    void clearAll();

private:
    std::vector<std::string> resolveActiveNames(const Event& event);
    std::vector<std::string> resolveClearNames(const Event& event);
    void executeNames(const std::vector<std::string>& names);

    // name → callback
    std::unordered_map<std::string, ActionCallback> actionTable_;

    // EventId → (activeNames, clearNames)
    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > > eventConfig_;

    // level → default names
    std::unordered_map<int, std::vector<std::string> > levelDefaults_;

    // 未注册动作的 fallback（宿主注入）
    FallbackCallback fallback_;
};

#endif // LINKAGE_ENGINE_H
