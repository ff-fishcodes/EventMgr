#ifndef LINKAGE_ENGINE_H
#define LINKAGE_ENGINE_H

#include "event_types.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <QThreadPool>

// ============================================================
// LinkageEngine — 联动引擎（命名回调制）
//
// registerAction(name, callback)       — 注册能力
// configureEvent(id, {names}, {names}) — 绑定能力到事件
// setLevelDefault(level, {names})      — 等级默认动作
//
// 运行时: addEvent → resolveNames → 查 actionTable_ → 线程池异步执行
// ============================================================
class LinkageEngine {
public:
    using ActionCallback   = std::function<void()>;
    using FallbackCallback = std::function<void(const std::string& eventId, bool isActive)>;

    // 单例
    static LinkageEngine& instance();
    static void setInstance(LinkageEngine* eng);

    LinkageEngine();
    ~LinkageEngine() {}

    LinkageEngine(const LinkageEngine&) = delete;
    LinkageEngine& operator=(const LinkageEngine&) = delete;

    void registerAction(const std::string& name, ActionCallback callback);
    void setFallback(FallbackCallback callback);
    void configureEvent(const EventId& eventId,
                        const std::vector<std::string>& activeNames,
                        const std::vector<std::string>& clearNames);
    void setLevelDefault(EventLevel level,
                         const std::vector<std::string>& names);

    void executeActive(const Event& event);
    void executeCleared(const Event& event);
    void clearAll();

private:
    std::vector<std::string> resolveActiveNames(const Event& event);
    std::vector<std::string> resolveClearNames(const Event& event);
    void executeNames(const std::vector<std::string>& names);

    std::unordered_map<std::string, ActionCallback> actionTable_;
    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > > eventConfig_;
    std::unordered_map<int, std::vector<std::string> > levelDefaults_;
    FallbackCallback fallback_;

    // 联动动作独享线程池，不与全局共用，避免被其他 Qt 任务挤占
    QThreadPool linkagePool_;

    static LinkageEngine* instance_;
};

#endif // LINKAGE_ENGINE_H
