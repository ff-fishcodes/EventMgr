#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H

#include <string>
#include <vector>

// ============================================================
// 事件等级 — 与项目配置模块报警等级对应
// 1=紧急, 2=严重, 3=一般, 4=提示
// ============================================================
enum class EventLevel {
    Emergency = 1,
    Serious   = 2,
    General   = 3,
    Info      = 4
};

// ============================================================
// 事件状态 — 产生/消除两状态模型
// ============================================================
enum class EventState {
    Active,
    Cleared
};

// ============================================================
// 联动动作 — 告警产生/消除时的执行单元
// 一个事件可携带多个联动动作
// ============================================================
struct LinkageAction {
    enum Type {
        LockUI,       // 锁前端控件    target=控件标识
        UnlockUI,     // 解锁前端控件  target=控件标识
        SendCommand,  // 向下位机发指令 target=指令名 param=指令内容
        Buzzer        // 蜂鸣器提示    target=模式名 param=参数（预留）
        // 后续新增联动类型只需追加枚举值，注册新 handler 即可
    };

    Type        type;
    std::string target;   // UI控件标识 / 指令名称 / 蜂鸣器模式
    std::string param;    // 附加参数

    LinkageAction() : type(LockUI) {}
    LinkageAction(Type t, const std::string& tg, const std::string& p = "")
        : type(t), target(tg), param(p) {}
};

// ============================================================
// 事件编号 — "protocolID-frameID-报警字段" 组合键
// ============================================================
using EventId = std::string;

// ============================================================
// 事件值对象 — 自包含，创建时指定等级和联动内容
// ============================================================
struct Event {
    EventId     id;               // "protocolID-frameID-alarmField"
    int         protocolID;
    int         frameID;
    std::string alarmField;       // 报警字段名
    std::string description;      // 报警描述（eg: "下位机1-温度过高"）
    EventLevel  originalLevel;    // 业务创建时指定的原始等级
    EventLevel  effectiveLevel;   // 经降级后的实际等级（addEvent 时计算）
    EventState  state;

    std::vector<LinkageAction> activeActions;   // 告警产生时的联动列表
    std::vector<LinkageAction> clearActions;    // 告警消除时的联动列表

    Event() : protocolID(0), frameID(0),
              originalLevel(EventLevel::Info),
              effectiveLevel(EventLevel::Info),
              state(EventState::Active) {}
};

#endif // EVENT_TYPES_H
