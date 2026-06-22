#ifndef EXTERNAL_API_H
#define EXTERNAL_API_H

#include "event_types.h"

class EventManager;

// ============================================================
// ExternalAPI — 对外接口层（本需求的唯一门面）
//
// 职责:
//   - 工厂方法: createAlarm() 创建事件值对象
//   - 入口方法: addEvent() / clearEvent()
//
// 所有外部模块（NetworkReceiver、通信监测等）仅通过此类与本需求交互
// ============================================================
class ExternalAPI {
public:
    explicit ExternalAPI(EventManager& eventMgr);

    // ========= 工厂方法 =========

    // 创建告警事件值对象
    // 调用方拿到 Event 后可设置联动内容（activeActions / clearActions），
    // 然后调用 addEvent 提交
    Event createAlarm(int protocolID, int frameID,
                      const std::string& alarmField,
                      EventLevel level,
                      const std::string& description);

    // ========= 入口方法 =========

    // 告警产生：提交事件，接口稳定不随 Event 字段变更而改动
    void addEvent(const Event& event);

    // 告警消除：仅需定位键即可清除
    void clearEvent(int protocolID, int frameID, const std::string& alarmField);

private:
    EventManager& eventMgr_;
};

#endif // EXTERNAL_API_H
