# 事件管理中心 — 设计方案

## 1. 概述

### 1.1 需求背景

上位机从多个下位机接收工作状态信息，解析其中的健康信息用于监测管理下位机健康状态，并根据健康状态等级执行不同的管控动作。

### 1.2 关键约束

| 约束项 | 内容 |
|--------|------|
| 运行平台 | 飞腾 FT/2000 |
| 操作系统 | 银河麒麟 V10 |
| 开发语言 | C++11 |
| 开发工具 | Qt 5.8.12 |
| 前后端分离 | 前端 Qt，后端不依赖 Qt 库 |
| 通信方式 | 上位机与下位机网络传输；前后端本地 Socket |

### 1.3 已有基础设施（本需求不实现，以桩/伪代码表示）

| 模块 | 说明 |
|------|------|
| NetworkReceiver | 接收下位机状态帧，解析报警字段，调用本需求 addEvent/clearEvent 接口 |
| Socket 服务端 | 前后端通信，本需求调用其接口推送事件变更通知给前端 |
| LogWriter | 日志写入，本需求调用其接口写入事件日志 |
| 通信监测 | 监测下位机通信状态，断联/恢复通过 addEvent/clearEvent 上报 |
| 管控指令发送 | 向下位机发送管控指令，本需求 LinkageEngine 调用其接口 |

### 1.4 已有数据标识

- `protocolID` — 区分不同下位机
- `frameID` — 区分同一设备的同一状态帧
- `protocolID + frameID + 报警字段` — 唯一确定一个告警事件
- 状态帧周期上报，频率 ≥1 Hz
- 项目配置模块可提供所有报警字段及对应等级、描述，本需求通过外部接口获取

---

## 2. 核心设计决策

| 决策项 | 结论 |
|--------|------|
| 事件编号 | `protocolID-frameID-报警字段` 组合键字符串 |
| 并发模型 | 单线程事件循环 |
| 持久化 | 不持久化，重启后从下位机上报重建 |
| 规模 | 中等（5-20 下位机，≤100 状态帧） |
| 防抖 | 不加防抖（信任下位机数据质量） |
| 事件状态 | 产生 / 消除 两状态，不需要确认状态 |
| 告警降级 | 持久降级，精确到 protocolID+frameID+报警字段 |
| 报警屏蔽 | 仅 UI 隐藏，后台继续记录和联动；UI 显示屏蔽计数提示 |
| 降级与屏蔽 | 可独立或同时对同一事件生效 |
| 管控动作 | 当前：UI 锁定/解锁 + 向下位机发指令；其他等级预留接口 |

---

## 3. 整体架构

```
┌──────────────────────────────────────────────────┐
│                    上位机                         │
│                                                  │
│  ┌──────────────┐   本地Socket     ┌────────────┐ │
│  │  前端 (Qt)   │◄───────────────►│ 后端 (C++)  │ │
│  │              │                 │             │ │
│  │ · 事件列表   │                 │ · 事件管理中心│ │
│  │ · 配置管理UI │                 │ · 联动引擎  │ │
│  │ · 颜色分级   │                 │ · 配置管理  │ │
│  └──────────────┘                 └──────┬──────┘ │
│                                         │         │
│                             已有: 网络层(protocolID区分)│
│                             ┌───┬───┬───┐    │
│                             ▼   ▼   ▼   ▼    │
│                          下位机1 下位机2 ...下位机N  │
└──────────────────────────────────────────────────┘
```

---

## 4. 本需求模块设计

### 4.1 模块总览

```
                    ┌─────────────────────┐
已有: NetworkReceiver│                    │ 已有: Socket服务端
      ──────────────►│  ExternalAPI       ├──────────────► 前端
                     │                    │
已有: 通信监测       │  EventManager      │  已有: LogWriter
      ──────────────►│  LinkageEngine     ├──────────────► 日志
                     │  ConfigManager     │
                     │                    │  已有: 管控指令发送
                     │                    ├──────────────► 下位机
                     └─────────────────────┘
```

### 4.2 ExternalAPI（对外接口层）

本需求对外暴露的核心接口。所有事件（告警产生、消除、下位机断联/恢复）统一通过此接口进入。

```cpp
// 事件编号生成
// EventId = std::to_string(protocolID) + "-" + std::to_string(frameID) + "-" + alarmField

// 告警产生
// 输入: protocolID, frameID, alarmField(报警字段名), level(1-4), description(报警描述)
// 输出: 无
void addEvent(int protocolID, int frameID, const std::string& alarmField,
              int level, const std::string& description);

// 告警消除
// 输入: protocolID, frameID, alarmField
// 输出: 无
void clearEvent(int protocolID, int frameID, const std::string& alarmField);
```

**设计要点：**
- 断联作为一种事件，与普通告警统一走 addEvent / clearEvent 入口，不单独设接口
- 调用方（已有模块）负责传参，本模块不关心事件来源是下位机报警还是通信监测

### 4.3 EventManager（事件管理中心）

核心数据结构，管理所有活跃事件的增删查。

```cpp
// 事件等级
enum class EventLevel { Emergency = 1, Serious = 2, General = 3, Info = 4 };

// 事件状态
enum class EventState { Active, Cleared };

// 事件编号
using EventId = std::string;  // "protocolID-frameID-alarmField"

// 事件结构
struct Event {
    EventId     id;
    int         protocolID;
    int         frameID;
    std::string alarmField;
    std::string description;
    EventLevel  originalLevel;      // 配置模块给定的原始等级
    EventLevel  effectiveLevel;     // 经降级配置调整后的实际等级
    EventState  state;
};

// 核心存储
std::unordered_map<EventId, Event> activeEvents;
```

**处理流程（addEvent）：**

```
addEvent(id, level, desc)
  │
  ▼
EventId = "protocolID-frameID-alarmField"
  │
  ▼
查 activeEvents.find(EventId)
  │
  ├── 已存在 → 无操作，直接返回
  │
  └── 不存在:
        │
        ▼
      effectiveLevel = ConfigManager.getEffectiveLevel(EventId, level)
        │
        ▼
      创建 Event 对象，插入 activeEvents
        │
        ▼
      LinkageEngine.onAlarmActive(event)
        │
        ▼
      if (!ConfigManager.isShielded(EventId)):
        调用 Socket服务端 → 通知前端
        │
        ▼
      调用 LogWriter → 写日志: "{下位机}-{故障名}-发生"
```

**处理流程（clearEvent）：**

```
clearEvent(protocolID, frameID, alarmField)
  │
  ▼
EventId = "protocolID-frameID-alarmField"
  │
  ▼
查 activeEvents.find(EventId)
  │
  ├── 不存在 → 无操作，直接返回
  │
  └── 存在:
        │
        ▼
      event.state = Cleared
        │
        ▼
      LinkageEngine.onAlarmCleared(event)
        │
        ▼
      调用 Socket服务端 → 通知前端移除
        │
        ▼
      调用 LogWriter → 写日志: "{下位机}-{故障名}-消除"
        │
        ▼
      activeEvents.erase(EventId)
```

### 4.4 ConfigManager（配置管理）

管理降级和屏蔽两项配置，两者独立作用可叠加。

```cpp
// 降级映射: EventId → 降级后的等级
std::unordered_map<EventId, EventLevel> downgradeMap;

// 屏蔽集合: 被屏蔽的 EventId
std::unordered_set<EventId> shieldSet;

// ========= 降级相关 =========

// 设置降级
void setDowngrade(const EventId& id, EventLevel newLevel);

// 取消降级
void removeDowngrade(const EventId& id);

// 获取有效等级（若有降级配置则返回降级后等级，否则返回原始等级）
EventLevel getEffectiveLevel(const EventId& id, EventLevel originalLevel);

// ========= 屏蔽相关 =========

// 设置屏蔽
void setShield(const EventId& id);

// 取消屏蔽
void clearShield(const EventId& id);

// 查询是否被屏蔽
bool isShielded(const EventId& id);

// 获取当前被屏蔽的事件数量（用于前端屏蔽计数提示）
int getShieldCount();
```

**降级与屏蔽叠加效果：**

```
originalLevel
    │
    ▼
降级映射 → effectiveLevel (决定联动行为 + 日志记录)
    │
    ▼
屏蔽检查 → 决定是否通知前端 (屏蔽=不通知, 但联动和日志照常)
```

**初始化：** 每次启动从项目配置模块加载已有配置（无需持久化）。

**UI操作入口（前端实现，本需求只提供接口）：**
- 事件已产生时：界面提供降级操作按钮
- 事件未产生时：加载所有可能的报警字段，通过复选框设置降级/屏蔽

### 4.5 LinkageEngine（联动引擎）

根据事件等级执行对应的联动动作。当前仅紧急故障有实际行动，其他等级预留接口。

```cpp
// 告警产生时的联动
void onAlarmActive(const Event& event) {
    switch (event.effectiveLevel) {
        case EventLevel::Emergency:
            调用已有模块 → UI锁定
            调用已有模块 → 向下位机发送管控指令(event.protocolID)
            break;
        case EventLevel::Serious:
        case EventLevel::General:
        case EventLevel::Info:
            // 当前仅UI提示，预留扩展
            break;
    }
}

// 告警消除时的联动
void onAlarmCleared(const Event& event) {
    switch (event.effectiveLevel) {
        case EventLevel::Emergency:
            调用已有模块 → UI解锁
            // 预留: 蜂鸣器提示、解除管控指令等
            break;
        default:
            // 预留扩展
            break;
    }
}
```

**设计要点：**
- 每个 case 预留扩展点，后续可追加动作（蜂鸣器、更多管控指令等）
- 联动动作的执行（UI锁定、管控指令、蜂鸣器等）均由已有模块负责，LinkageEngine 只负责决策和调用

---

## 5. 数据结构设计

### 5.1 事件存储

```cpp
// O(1) 查找：验证重复上报
std::unordered_map<EventId, Event> activeEvents;

// 辅助索引：按 protocolID 快速查找某下位机所有事件（方便断联时批量处理）
// 选做：规模不大时遍历 activeEvents 也可
```

### 5.2 ConfigManager 存储

```cpp
std::unordered_map<EventId, EventLevel> downgradeMap;  // 降级表
std::unordered_set<EventId> shieldSet;                  // 屏蔽集
```

### 5.3 前端通信协议（本地Socket）

后端通过已有Socket服务端向前端推送事件变更，推送内容：

```json
{
    "type": "alarm_active" | "alarm_cleared",
    "protocolID": 1,
    "frameID": 3,
    "alarmField": "temp_high",
    "description": "下位机1-温度过高",
    "effectiveLevel": 2,
    "shielded": false
}
```

---

## 6. 接口清单汇总

### 6.1 本需求对外提供（ExternalAPI）

| 接口 | 说明 |
|------|------|
| `addEvent(protocolID, frameID, alarmField, level, desc)` | 告警产生 |
| `clearEvent(protocolID, frameID, alarmField)` | 告警消除 |

### 6.2 ConfigManager 对外提供

| 接口 | 说明 |
|------|------|
| `setDowngrade(eventId, level)` | 设置降级 |
| `removeDowngrade(eventId)` | 取消降级 |
| `setShield(eventId)` | 设置屏蔽 |
| `clearShield(eventId)` | 取消屏蔽 |
| `getShieldCount()` | 获取屏蔽数量 |

### 6.3 本需求对外调用（已有模块接口，桩/伪代码）

| 接口 | 说明 |
|------|------|
| `SocketServer::notifyFrontend(json)` | 向前端推送事件变更 |
| `LogWriter::write(msg)` | 写日志 |
| `UIControl::lock() / unlock()` | UI锁定/解锁 |
| `CmdSender::send(protocolID, cmd)` | 向下位机发管控指令 |

---

## 7. 前端设计要点（Qt）

| 功能点 | 说明 |
|--------|------|
| 事件列表展示 | 接收后端Socket推送，实时更新，不同等级不同颜色 |
| 事件格式 | 显示 "{下位机}-{故障描述}" |
| 颜色分级 | 紧急=红色, 严重=橙色, 一般=黄色, 提示=蓝色 |
| 降级操作 | 已产生事件：界面提供降级操作入口 |
| 屏蔽配置 | 未产生事件：加载所有报警字段，复选框设置降级/屏蔽 |
| 屏蔽提示 | 状态栏显示"当前N个报警被屏蔽" |
| 清除展示 | 事件消除时从列表中移除 |

---

## 8. 边界与范围

**本需求实现：** EventManager、LinkageEngine、ConfigManager、ExternalAPI

**已有模块（桩/伪代码）：** NetworkReceiver、Socket服务端、LogWriter、通信监测、管控指令发送
