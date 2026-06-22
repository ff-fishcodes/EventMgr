# 事件管理中心 — 软件设计说明

> 版本：v1.1
> 日期：2026-06-17
> v1.0 → v1.1：LinkageEngine per-protocolID 注册、前后端兼容适配、notifyFrontend 参数 bool 化

---

## 1. 引言

### 1.1 编写目的

本文档为"事件管理中心"模块的软件设计说明，供开发、测试和维护人员参阅。

### 1.2 适用范围

本文档适用于飞腾 FT/2000 平台、银河麒麟 V10 操作系统下的上位机事件管理子系统。

### 1.3 术语与缩写

| 术语 | 说明 |
|------|------|
| 上位机 | 本系统，负责汇聚并管理多个下位机的健康状态 |
| 下位机 | 被监控设备，通过周期状态帧上报健康信息 |
| protocolID | 下位机通信协议标识，用于区分不同下位机 |
| frameID | 状态帧标识，用于区分同一设备的不同状态帧 |
| EventId | 事件编号，格式 `"protocolID-frameID-报警字段"` |
| 联动 | 告警产生/消除时自动执行的管控动作 |
| 降级 | 降低报警等级的配置操作 |
| 屏蔽 | 隐藏 UI 报警展示的配置操作 |

### 1.4 参考文档

- 《事件管理需求》`doc/requirment.md`
- 《设计方案》`docs/superpowers/specs/2026-06-17-event-mgr-design.md`
- 《需求讨论记录》`docs/superpowers/specs/2026-06-17-requirement-discussion-record.md`

---

## 2. 软件概述

### 2.1 功能描述

事件管理中心接收下位机的报警信息，进行事件编号、分级、联动、记录和 UI 展示。支持告警产生、告警消除、报警降级、报警屏蔽四项核心功能。

### 2.2 运行环境

| 项目 | 规格 |
|------|------|
| CPU | 飞腾 FT/2000 |
| 操作系统 | 银河麒麟 V10 |
| 编译器 | GCC 支持 C++11 |
| 依赖库 | 后端纯 C++11（无 Qt 依赖）；前端 Qt 5.8.12 |

### 2.3 规模假设

- 下位机数量：5-20 台
- 状态帧总数：≤ 100
- 上报频率：≥ 1 Hz

---

## 3. 软件结构

### 3.1 模块划分

```
EventMgr/
├── main.cpp                        # 使用示例 / 入口
└── backend/
    ├── event_types.h               # 核心数据类型定义
    ├── config_manager.h/.cpp       # 配置管理（降级、屏蔽）
    ├── linkage_engine.h/.cpp       # 联动引擎（回调注册制，支持 per-protocolID）
    ├── event_manager.h/.cpp        # 事件管理中心（存储、查重、协调）
    ├── external_api.h/.cpp         # 对外接口层（工厂方法、入口）
    ├── setup.h/.cpp                # handler 注册初始化（全局 LockUI/UnlockUI/Buzzer）
    ├── frontend_adapter.h          # 前后端适配层（分离/一体两种模式）
    ├── device_controllers_example.h # 示例：各下位机控制单例注册模式
    └── stubs/
        ├── socket_server.h/.cpp    # 桩：前后端通信
        ├── log_writer.h/.cpp       # 桩：日志写入
        ├── cmd_sender.h/.cpp       # 桩：管控指令发送
        └── buzzer_control.h/.cpp    # 桩：蜂鸣器控制
```

### 3.2 模块依赖关系

```
main / 外部调用方
    │
    ▼
ExternalAPI  ──────────────────────────────────────────────┐
    │                                                     │
    ▼                                                     │
EventManager ──── ConfigManager ──── event_types.h        │
    │          ─── LinkageEngine  ─── event_types.h       │
    │          ─── SocketServer (桩)                       │
    │          ─── LogWriter (桩)                          │
    │                                                     │
setup.cpp ────── LinkageEngine                            │
    │          ─── SocketServer (桩)                       │
    │          ─── CmdSender (桩)                          │
    │          ─── BuzzerControl (桩)                      │
    │                                                     │
桩模块 (stubs/) ─── 无依赖（独立模块）                      │
```

---

## 4. 模块详细设计

### 4.1 event_types.h — 核心数据类型

**文件：** `backend/event_types.h`

定义本需求所有模块共用的枚举和结构体。

#### 枚举：EventLevel

```cpp
enum class EventLevel {
    Emergency = 1,   // 紧急故障
    Serious   = 2,   // 严重故障
    General   = 3,   // 一般故障
    Info      = 4    // 提示
};
```

#### 枚举：EventState

```cpp
enum class EventState { Active, Cleared };
```

#### 结构体：LinkageAction

联动动作的最小执行单元。

| 字段 | 类型 | 说明 |
|------|------|------|
| type | enum Type | 动作类型：LockUI, UnlockUI, SendCommand, Buzzer |
| target | std::string | 语义目标标识（控件名/指令名/模式名） |
| param | std::string | 附加参数 |

#### 结构体：Event

事件值对象，自包含所有信息。

| 字段 | 类型 | 说明 |
|------|------|------|
| id | EventId (std::string) | `"protocolID-frameID-alarmField"` |
| protocolID | int | 下位机标识 |
| frameID | int | 状态帧标识 |
| alarmField | std::string | 报警字段名 |
| description | std::string | 报警描述 |
| originalLevel | EventLevel | 业务创建时指定的原始等级 |
| effectiveLevel | EventLevel | 经降级后的实际等级 |
| state | EventState | 当前状态 |
| activeActions | vector\<LinkageAction\> | 告警产生时的联动列表 |
| clearActions | vector\<LinkageAction\> | 告警消除时的联动列表 |

---

### 4.2 ExternalAPI — 对外接口层

**文件：** `backend/external_api.h`, `backend/external_api.cpp`

本需求的唯一对外门面，所有外部模块仅通过此类交互。

#### 接口说明

| 方法 | 说明 |
|------|------|
| `createAlarm(protocolID, frameID, alarmField, level, desc) → Event` | 工厂方法：创建事件值对象，自动生成 EventId |
| `addEvent(const Event&)` | 告警产生：提交事件到 EventManager |
| `clearEvent(protocolID, frameID, alarmField)` | 告警消除：提交清除请求 |

#### 设计要点

- `createAlarm` 仅创建 Event 对象，不做业务处理。调用方拿到 Event 后设置联动内容，再调 `addEvent`
- `addEvent` 入参为 Event 值对象，接口不随 Event 字段变更而改动
- 断联、恢复等所有事件类型统一走 `createAlarm + addEvent` / `clearEvent`

---

### 4.3 EventManager — 事件管理中心

**文件：** `backend/event_manager.h`, `backend/event_manager.cpp`

核心存储与调度模块，负责活跃事件的增删查。

#### 核心存储

```cpp
std::unordered_map<EventId, Event> activeEvents_;  // O(1) 查找
```

#### 构造函数

```cpp
// 默认构造：走 SocketServer 桩（向后兼容）
EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng);

// 注入前端通知回调：切换分离/一体模式
EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng,
             NotifyCallback notifyCb);
```

#### 方法说明

| 方法 | 说明 |
|------|------|
| `processAddEvent(event)` | 查重 → 计算 effectiveLevel → 存储 → 触发联动 → 通知前端 → 写日志 |
| `processClearEvent(protocolID, frameID, alarmField)` | 查找 → 触发联动(消除侧) → 通知 → 日志 → 移除 |
| `findEvent(id) → const Event*` | 按 EventId 查找 |
| `activeCount() → size_t` | 返回当前活跃事件数 |
| `findEventsByProtocolID(protocolID) → vector<Event>` | 按下位机查找所有活跃事件 |

#### 处理流程（processAddEvent）

```
1. 查重：activeEvents_.find(event.id) → 已存在则直接返回
2. effectiveLevel = configMgr_.getEffectiveLevel(id, originalLevel)
3. 存入 activeEvents_
4. linkageEng_.executeActive(event)      ← 所有联动动作都会执行
5. 若未被屏蔽 → SocketServer 通知前端    ← 仅 UI 通知受屏蔽影响
6. LogWriter 写日志                       ← 日志不受屏蔽影响
```

#### 处理流程（processClearEvent）

```
1. 查 EventId = makeEventId(protocolID, frameID, alarmField)
2. activeEvents_.find(id) → 不存在则直接返回
3. event.state = Cleared
4. linkageEng_.executeCleared(event)
5. SocketServer 通知前端移除
6. LogWriter 写日志
7. activeEvents_.erase(id)
```

---

### 4.4 LinkageEngine — 联动引擎

**文件：** `backend/linkage_engine.h`, `backend/linkage_engine.cpp`

回调注册制联动引擎。遍历 Event 的 LinkageAction 列表，按类型查已注册 handler 并依次调用。

#### 方法说明

| 方法 | 说明 |
|------|------|
| `registerHandler(type, handler)` | 全局注册：匹配所有 protocolID（LockUI/UnlockUI/Buzzer 用） |
| `registerHandler(type, protocolID, handler)` | 按设备注册：仅匹配该 protocolID（SendCommand 用） |
| `executeActive(event)` | 执行 event.activeActions，先全局后设备 |
| `executeCleared(event)` | 执行 event.clearActions，先全局后设备 |

#### Handler 注册（setup.cpp）

| 动作类型 | Handler 行为 | 调用的模块 |
|----------|-------------|-----------|
| LockUI | 构造 JSON → SocketServer::notifyFrontend | SocketServer (桩) |
| UnlockUI | 构造 JSON → SocketServer::notifyFrontend | SocketServer (桩) |
| SendCommand | CmdSender::send(protocolID, target, param) | CmdSender (桩) |
| Buzzer | BuzzerControl::play(target, param) | BuzzerControl (桩) |

**扩展方式：** 新增联动类型只需在 LinkageAction::Type 加枚举值，在 setup.cpp 中加一行 `registerHandler`，LinkageEngine 本身零改动。

---

### 4.5 ConfigManager — 配置管理

**文件：** `backend/config_manager.h`, `backend/config_manager.cpp`

管理降级和屏蔽两项配置。

#### 方法说明

| 方法 | 说明 |
|------|------|
| `setDowngrade(id, newLevel)` | 设置降级 |
| `removeDowngrade(id)` | 取消降级 |
| `getEffectiveLevel(id, originalLevel) → EventLevel` | 获取有效等级 |
| `setShield(id)` | 设置屏蔽 |
| `clearShield(id)` | 取消屏蔽 |
| `isShielded(id) → bool` | 查询屏蔽状态 |
| `getShieldCount() → int` | 获取屏蔽数量 |

#### 降级与屏蔽叠加效果

```
originalLevel（业务指定）
    │
    ▼
降级映射 → effectiveLevel → 影响: 联动行为 + 日志记录
    │
    ▼
屏蔽检查 → 影响: 是否通知前端（联动和日志照常）
```

---

### 4.6 桩模块

本需求不实现的已有模块，提供桩接口供编译和测试。

| 模块 | 接口 | 说明 |
|------|------|------|
| SocketServer | `notifyFrontend(json)` | 向前端推送事件变更 |
| LogWriter | `write(msg)` | 写日志 |
| CmdSender | `send(protocolID, target, param)` | 向下位机发管控指令 |
| BuzzerControl | `play(target, param)` | 蜂鸣器控制（预留） |

桩实现当前仅打印到 stdout，实际项目中替换为真实实现。

---

## 5. 数据设计

### 5.1 事件存储结构

```cpp
// Key: EventId = "protocolID-frameID-alarmField"
// Value: Event 值对象
std::unordered_map<EventId, Event> activeEvents;
```

- O(1) 查找，用于 `processAddEvent` 时的重复检查
- 消除时 erase 移除，不保留历史

### 5.2 配置存储结构

```cpp
// 降级映射
std::unordered_map<EventId, EventLevel> downgradeMap;

// 屏蔽集合
std::unordered_set<EventId> shieldSet;
```

### 5.3 Handler 注册表

```cpp
// Key: LinkageAction::Type 转 int
// Value: 注册的 handler 列表（支持同一类型多个 handler）
std::unordered_map<int, std::vector<ActionHandler>> handlers_;
```

---

## 6. 通信协议

### 6.1 前后端 Socket 消息格式

**告警产生：**

```json
{
    "type": "alarm_active",
    "protocolID": 1,
    "frameID": 3,
    "alarmField": "temp_high",
    "description": "下位机1-温度过高",
    "level": 1,
    "uiActions": [
        {"action": "lock_ui", "target": "panel_operation"}
    ]
}
```

**告警消除：**

```json
{
    "type": "alarm_cleared",
    "protocolID": 1,
    "frameID": 3,
    "alarmField": "temp_high",
    "uiActions": [
        {"action": "unlock_ui", "target": "panel_operation"}
    ]
}
```

### 6.2 uiActions 说明

- `uiActions` 由 EventManager 在通知前端时从 Event 的 activeActions/clearActions 中提取 LockUI/UnlockUI 类型动作生成
- `target` 为语义标识（如 `"panel_operation"`），前端维护 target → QWidget 映射表
- 前端收到消息后遍历 uiActions，匹配 target 执行对应控件操作

---

## 7. 并发设计

采用单线程事件循环模型。理由：

- 10×20Hz = 200 msg/s，处理耗时 ≤ 10ms/s
- 事件管理中心是共享状态的汇聚点，单线程天然避免锁竞争
- 阻塞 I/O 操作（管控指令发送）通过非阻塞方式处理

---

## 8. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 重复上报同一事件 | processAddEvent 中查重后静默忽略 |
| 消除不存在的事件 | processClearEvent 中查不到后静默忽略 |
| 无效等级降级设置 | ConfigManager 中检查范围并忽略 |
| Handler 未注册的 action type | LinkageEngine 中找不到 handler 则跳过 |

策略：对异常输入不抛异常，静默处理，保证系统稳定运行。

---

## 9. 编译构建

### 9.1 编译命令

```bash
g++ -std=c++11 -Wall -Wextra -o eventmgr \
  main.cpp \
  backend/external_api.cpp \
  backend/event_manager.cpp \
  backend/linkage_engine.cpp \
  backend/config_manager.cpp \
  backend/setup.cpp \
  backend/stubs/socket_server.cpp \
  backend/stubs/log_writer.cpp \
  backend/stubs/cmd_sender.cpp \
  backend/stubs/buzzer_control.cpp \
  -I.
```

### 9.2 编译要求

- C++11 标准
- 无 Qt 依赖（后端）
- 仅依赖 STL

---

## 10. 测试验证

`main.cpp` 包含 6 个演示场景，覆盖所有核心功能：

| 场景 | 验证点 |
|------|--------|
| 演示1：创建紧急告警 | addEvent → LockUI handler + SendCommand handler + 前端通知 + 日志 |
| 演示2：重复上报 | 查重有效，活跃事件数不变 |
| 演示3：消除事件 | UnlockUI handler + 前端通知 + 日志 + erase |
| 演示4：降级配置 | Emergency→Info，effectiveLevel 变更为 4 |
| 演示5：屏蔽配置 | 后端联动和日志照常，前端通知被抑制 |
| 演示6：断联事件 | frameID=0，统一走 createAlarm+addEvent |
