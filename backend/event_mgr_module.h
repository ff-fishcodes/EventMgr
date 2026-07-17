#ifndef EVENT_MGR_MODULE_H
#define EVENT_MGR_MODULE_H

#include "external_api.h"
#include "config_manager.h"
#include "linkage_engine.h"
#include "event_manager.h"

// ============================================================
// EventMgrModule — 一键启动事件管理模块
//
// 用法:
//   EventMgrModule::init();                          // 启动
//   ExternalAPI& api = EventMgrModule::api();        // 获取接口
//
// observe 组件:
//   EventMgrModule::api().triggerAlarm("锅炉", 3, "temp_high", true);
// ============================================================
class EventMgrModule {
public:
    // 初始化整个事件管理模块（启动时调用一次）
    static void init();

    // 获取 ExternalAPI 引用
    static ExternalAPI& api();

    // 获取 ConfigManager 引用（供前端配置页使用）
    static ConfigManager& config();

private:
    static ConfigManager*  configMgr_;
    static LinkageEngine*  linkageEng_;
    static EventManager*   eventMgr_;
    static ExternalAPI*    api_;
};

#endif // EVENT_MGR_MODULE_H
