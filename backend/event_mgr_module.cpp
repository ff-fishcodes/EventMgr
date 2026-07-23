#include "event_mgr_module.h"

ConfigManager*  EventMgrModule::configMgr_  = NULL;
LinkageEngine*  EventMgrModule::linkageEng_ = NULL;
EventManager*   EventMgrModule::eventMgr_   = NULL;
ExternalAPI*    EventMgrModule::api_        = NULL;

void EventMgrModule::init() {
    if (api_) return;  // 已初始化

    configMgr_  = new ConfigManager();
    linkageEng_ = new LinkageEngine();
    eventMgr_   = new EventManager(*configMgr_, *linkageEng_);
    api_        = new ExternalAPI(*eventMgr_, *configMgr_);

    // 注册为单例，供外部线程通过 instance() 获取
    ConfigManager::setInstance(configMgr_);
    LinkageEngine::setInstance(linkageEng_);
    EventManager::setInstance(eventMgr_);
    ExternalAPI::setInstance(api_);

    // 联动能力/事件配置/系统事件由业务代码在 init() 之后自行注册，
    // 通过 ExternalAPI 门面完成，不直接依赖 LinkageEngine。
}

ExternalAPI& EventMgrModule::api() {
    return *api_;
}

ConfigManager& EventMgrModule::config() {
    return *configMgr_;
}
