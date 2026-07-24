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
// 事件来源 — 区分下位机上报 vs 后端自行监测
// ============================================================
enum class EventSource {
    Device,   // 下位机状态帧上报
    System,   // 后端监测（通信断连、磁盘满、CPU 过载等）
    Unknown   // 未在目录/系统事件中定义的来源
};

// ============================================================
// 事件编号
//   格式: "deviceName-frameID-字段名"
//   设备事件: "锅炉-3-temp_high"
//   关联事件: "锅炉-0-comm_lost" (frameID=0)
//   系统事件: "系统-0-disk_full"  (frameID=0, deviceName 用户自定义)
//   本模块不做 deviceName 校验
// ============================================================
using EventId = std::string;

// ============================================================
// 报警定义 — 前端报警目录的每一行
// ============================================================
struct AlarmDef {
    EventId     id;               // "deviceName-frameID-alarmField"
    std::string description;      // 报警描述
    EventLevel  originalLevel;    // 原始等级
    bool        downgraded;       // 是否已降级
    EventLevel  downgradeTo;      // 降级到哪个等级（仅 downgraded=true 时有效）
    bool        shielded;         // 是否已屏蔽
    bool        isSystem;         // 是否为系统事件（前端分组排序用）

    AlarmDef() : originalLevel(EventLevel::Info),
                 downgraded(false), downgradeTo(EventLevel::Info),
                 shielded(false), isSystem(false) {}
};

// ============================================================
// 事件值对象 — 自包含，创建时指定等级，联动由引擎配置表决定
// ============================================================
struct Event {
    EventId     id;               // "deviceName-frameID-字段名"
    EventSource source;           // 事件来源
    std::string deviceName;       // 设备名（系统事件为自定义名，如"系统"）
    int         frameID;          // 状态帧标识（系统事件为 0）
    std::string alarmField;       // 报警字段名
    std::string description;      // 报警描述（eg: "锅炉-温度过高"）
    EventLevel  originalLevel;    // 业务创建时指定的原始等级
    EventLevel  effectiveLevel;   // 经降级后的实际等级（addEvent 时计算）
    EventState  state;
    std::string timestamp;        // 事件产生时间 "YYYY-MM-DD HH:MM:SS"

    // 兜底：如果引擎配置表和等级默认都没配，则用此列表
    std::vector<std::string> activeActions;
    std::vector<std::string> clearActions;

    Event() : source(EventSource::Device),
              frameID(0),
              originalLevel(EventLevel::Info),
              effectiveLevel(EventLevel::Info),
              state(EventState::Active) {}
};

#endif // EVENT_TYPES_H
