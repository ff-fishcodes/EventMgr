#ifndef FRONTEND_ADAPTER_H
#define FRONTEND_ADAPTER_H
// ============================================================
// 前端适配层 — 同一套后端核心，两种部署模式
//
// 模式A（前后端分离）：handler → Socket 通信 → 独立 Qt 进程
// 模式B（前后端一体）：handler → 直接调用 UI 方法 → 同进程 Qt
//
// 切换方式：编译时选择 A 或 B，或在初始化时选择注册哪套 handler。
// EventManager / LinkageEngine / ConfigManager / ExternalAPI 零改动。
// ============================================================

#include "event_types.h"
#include "linkage_engine.h"
#include <iostream>
#include <functional>

// ============================================================
// 前端回调接口 — 两种模式必须实现的契约
// ============================================================
struct FrontendCallbacks {
    // 控件锁定 / 解锁
    // target: 语义控件标识（如 "panel_main"）
    // locked: true=锁定, false=解锁
    std::function<void(const std::string& target, bool locked)> lockUI;

    // 事件列表变更（alarm_active / alarm_cleared）
    // jsonMsg: JSON 格式事件数据
    std::function<void(const std::string& jsonMsg)> updateEventList;
};

// ============================================================
// 模式A：前后端分离 — 通过 Socket 桩通信
// ============================================================
inline FrontendCallbacks createSocketBasedCallbacks() {
    FrontendCallbacks cb;

    cb.lockUI = [](const std::string& target, bool locked) {
        // 构造 JSON 通过 SocketServer 发送
        std::cout << "[Socket→前端] "
                  << (locked ? "lock_ui" : "unlock_ui")
                  << " target=" << target << std::endl;
        // 实际: SocketServer::notifyFrontend(json);
    };

    cb.updateEventList = [](const std::string& jsonMsg) {
        std::cout << "[Socket→前端] event: " << jsonMsg << std::endl;
        // 实际: SocketServer::notifyFrontend(jsonMsg);
    };

    return cb;
}

// ============================================================
// 模式B：前后端一体 — 直接调用 UI（Qt 或测试桩）
// ============================================================
inline FrontendCallbacks createDirectCallbacks() {
    FrontendCallbacks cb;

    cb.lockUI = [](const std::string& target, bool locked) {
        // 同进程内直接操作 Qt widget
        // QWidget* w = findWidgetByTarget(target);
        // w->setEnabled(!locked);
        std::cout << "[直接调用→Qt] "
                  << (locked ? "setEnabled(false)" : "setEnabled(true)")
                  << " on " << target << std::endl;
    };

    cb.updateEventList = [](const std::string& jsonMsg) {
        // 同进程内直接更新事件列表 UI
        // EventListWidget::updateFromJson(jsonMsg);
        std::cout << "[直接调用→Qt] 更新事件列表: " << jsonMsg << std::endl;
    };

    return cb;
}

// ============================================================
// 统一的 handler 注册 — 无论哪种模式，注册逻辑完全一致
// ============================================================
inline void registerUIHandlers(LinkageEngine& engine,
                                const FrontendCallbacks& cb) {
    // LockUI — 模式 A 走 Socket，模式 B 直接调 Qt，handler 代码一模一样
    engine.registerHandler(LinkageAction::LockUI,
        [cb](const Event& e, const LinkageAction& a) {
            cb.lockUI(a.target, true);
            // 两种模式都会额外收到 alarm_active 事件列表推送
            // 由 EventManager::notifyFrontend 负责，见下方说明
        });

    engine.registerHandler(LinkageAction::UnlockUI,
        [cb](const Event& e, const LinkageAction& a) {
            cb.lockUI(a.target, false);
        });
}

// ============================================================
// 使用示例
// ============================================================
inline void demoBothModes() {
    std::cout << "\n=== 模式A: 前后端分离 ===\n";
    {
        LinkageEngine engine;
        auto cb = createSocketBasedCallbacks();
        registerUIHandlers(engine, cb);

        Event e;
        e.id = "1-3-test";
        e.description = "test";
        e.activeActions.push_back(
            LinkageAction(LinkageAction::LockUI, "panel_main", ""));
        engine.executeActive(e);
    }

    std::cout << "\n=== 模式B: 前后端一体 ===\n";
    {
        LinkageEngine engine;
        auto cb = createDirectCallbacks();
        registerUIHandlers(engine, cb);

        Event e;
        e.id = "2-1-test";
        e.description = "test";
        e.activeActions.push_back(
            LinkageAction(LinkageAction::LockUI, "panel_aux", ""));
        engine.executeActive(e);
    }
}
// 输出:
// === 模式A ===
// [Socket→前端] lock_ui target=panel_main
//
// === 模式B ===
// [直接调用→Qt] setEnabled(false) on panel_aux

#endif
