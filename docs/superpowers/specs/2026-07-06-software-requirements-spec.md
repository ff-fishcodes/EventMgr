# 事件管理中心 — 需求规格说明文档

> 文档编号：EventMgr-RS-001
> 版本：v1.0
> 日期：2026-07-06
> 说明：本文档按照项目需求规格说明文档模板格式编写，不覆盖已有文档

---

## 1. 范围

### 1.1 标识

- **系统名称**：事件管理中心 (EventMgr)
- **文档编号**：EventMgr-RS-001
- **适用项目**：上位机事件管理子系统

### 1.2 概述

事件管理中心是上位机系统的核心子系统，负责接收来自多个下位机的健康状态信息，处理后端自行监测的系统事件，根据预定义规则生成告警、执行联动管控动作、实时呈现事件状态给操作员。

**核心能力**：
- 告警产生与消除
- 事件联动（告警触发时自动执行管控动作）
- 告警降级与屏蔽配置管理
- 系统事件支持（后端自行监测，如通信断连、磁盘满等）
- 前端实时展示与配置管理

**运行环境**：飞腾 FT/2000 平台，银河麒麟 V10 操作系统，后端 C++11（无 Qt 依赖），前端 Qt 5.15.2

**规模假设**：下位机 5-20 台，状态帧总数 ≤ 100，上报频率 ≥ 1 Hz

---

## 2. 引用文档

| 序号 | 文档名称 | 文档编号 | 版本 |
|------|---------|----------|------|
| 1 | 事件管理需求 | doc/requirment.md | — |
| 2 | 设计方案 | docs/superpowers/specs/2026-06-17-event-mgr-design.md | v5 |
| 3 | 需求讨论记录 | docs/superpowers/specs/2026-06-17-requirement-discussion-record.md | — |
| 4 | ActionRegistry 设计 | docs/superpowers/specs/2026-06-26-action-registry-design.md | — |
| 5 | 架构图 | docs/superpowers/specs/2026-06-25-architecture-diagrams.md | — |

---

## 3. 需求

### 3.1 能力需求（RX_BASIC_ABILITY）

#### 3.1.1 告警产生（RX_ALARM_GENERATE）

##### 3.1.1.1 功能需求

系统应能够接收来自下位机状态帧的报警信息，或后端自行监测的系统事件，生成唯一的告警事件，存入活跃事件表，并触发相应的联动动作和前端通知。

系统应支持两种事件来源：
- **设备事件（Device）**：下位机状态帧上报，EventId 格式为 `"protocolID-frameID-alarmField"`
- **系统事件（System）**：后端自行监测产生，EventId 直接使用预定义的事件名（如 `"disk_full"`），关联设备时使用 `"eventName:protocolID"` 格式

同 EventId 的重复上报应被静默忽略，不产生新事件、不重复触发联动。

##### 3.1.1.2 输入输出

**输入**：

| 接口 | 参数 | 说明 |
|------|------|------|
| `triggerAlarm(protocolID, frameID, alarmField, isActive)` | protocolID: int, frameID: int, alarmField: string, isActive: bool | observe 组件对接入口，自动查报警目录获取等级和描述 |
| `triggerSystemEvent(eventName, isActive)` | eventName: string, isActive: bool | 纯系统事件入口 |
| `triggerSystemEvent(eventName, protocolID, isActive)` | eventName: string, protocolID: int, isActive: bool | 关联设备系统事件入口 |
| `createAlarm(protocolID, frameID, alarmField, level, description) → Event` | level: EventLevel (1-4), description: string | 手动创建事件值对象 |
| `addEvent(Event)` | Event: 事件值对象 | 提交告警产生 |

**输出**：
- 活跃事件表新增一条记录
- 联动动作被执行（紧急/严重/一般等级时）
- 前端收到 `alarm_active` 通知（未被屏蔽时）
- 日志写入 `"下位机{ID}-{描述}-发生"`

##### 3.1.1.3 行为过程描述

```
1. 调用方通过 triggerAlarm / triggerSystemEvent / createAlarm+addEvent 发起
2. triggerAlarm 内部：构造 EventId → 查 AlarmCatalog 获取等级和描述 → createAlarm → addEvent
3. triggerSystemEvent 内部：查 system_events 预定义列表验证 → 构造 Event → addEvent
4. EventManager::processAddEvent：
   a. 查重：若 activeEvents_ 中已存在该 EventId，忽略
   b. 计算 effectiveLevel：查 ConfigManager 降级配置
   c. 生成 timestamp：当前时间 YYYY-MM-DD HH:MM:SS 格式
   d. 存入 activeEvents_
   e. 若 effectiveLevel ≠ Info，触发 LinkageEngine::executeActive
   f. 若未被屏蔽，通知前端
   g. 写日志
```

##### 3.1.1.4 行为参数

| 参数 | 约束 | 说明 |
|------|------|------|
| protocolID | ≥ 0 整数，System 事件可为 0 | 下位机标识 |
| frameID | ≥ 0 整数，System 事件为 0 | 状态帧标识 |
| alarmField | 非空字符串 | 报警字段名 |
| level | 1=Emergency, 2=Serious, 3=General, 4=Info | 事件等级 |
| timestamp | 自动生成，格式 "YYYY-MM-DD HH:MM:SS" | 精确到秒 |
| 响应时间 | 查重 O(1)，单次处理 ≤ 1ms | 性能约束 |

---

#### 3.1.2 告警消除（RX_ALARM_CLEAR）

##### 3.1.2.1 功能需求

系统应提供告警消除入口，调用方通过 `triggerAlarm(isActive=false)`、`triggerSystemEvent(isActive=false)` 或 `clearEvent` 接口提交消除请求。系统根据 EventId 查找活跃事件，执行消除联动、通知前端移除、写消除日志、从活跃事件表移除。

消除不存在的事件应静默忽略。

消除接口支持三种形式：
- `clearEvent(protocolID, frameID, alarmField)` — 按设备事件三字段消除
- `clearEvent(eventId)` — 按 EventId 直接消除，自动解析 Device/System 格式

##### 3.1.2.2 输入输出

**输入**：

| 接口 | 参数 | 说明 |
|------|------|------|
| `triggerAlarm(pid, fid, field, false)` | pid: int, fid: int, field: string | observe 消除 |
| `triggerSystemEvent(name, false)` | name: string | 系统事件消除 |
| `clearEvent(protocolID, frameID, alarmField)` | 三字段 | 设备事件消除 |
| `clearEvent(eventId)` | eventId: string | 按 ID 消除（自动解析格式） |

**输出**：
- 活跃事件表移除该记录
- 联动引擎执行消除侧动作
- 前端收到 `alarm_cleared` 通知
- 日志写入 `"下位机{ID}-{描述}-消除"`

##### 3.1.2.3 行为过程描述

```
1. 构造 EventId（Device 事件: "P-F-A"，System 事件: "eventName" 或 "eventName:P"）
2. EventManager::processClearEvent：
   a. 查 activeEvents_，不存在则忽略
   b. event.state = Cleared
   c. linkageEng_.executeCleared(event)
   d. 通知前端（alarm_cleared）
   e. 写消除日志
   f. activeEvents_.erase(id)
```

##### 3.1.2.4 行为参数

| 参数 | 约束 | 说明 |
|------|------|------|
| 消除不存在事件 | 静默忽略，不报错 | 容错设计 |
| 消除响应时间 | O(1) 查找 + 擦除 | 性能约束 |

---

#### 3.1.3 事件联动（RX_EVENT_LINKAGE）

##### 3.1.3.1 功能需求

系统应在告警产生/消除时自动执行预配置的联动管控动作。联动配置在系统启动时通过 ActionRegistry 集中完成，支持三层配置（事件级、等级级、实例级）和按有效等级控制。

联动动作以字符串名称为注册标识，通过 `ActionRegistry::setup()` 集中注册 Lambda 回调。

有效等级为 Info（提示）时，不执行任何联动动作。

##### 3.1.3.2 输入输出

**输入**：

| 方法 | 说明 |
|------|------|
| `registerAction(name, callback)` | 向引擎注册一个可执行动作 |
| `configureEvent(eventId, activeNames, clearNames)` | 为指定 EventId 配置联动动作名称列表 |
| `setLevelDefault(level, names)` | 为指定等级设置默认联动动作 |
| `executeActive(event)` / `executeCleared(event)` | 由 EventManager 调用，触发联动执行 |

**输出**：
- 已注册的回调函数按配置的名称列表依次执行
- 若名称未注册，跳过

##### 3.1.3.3 行为过程描述

```
executeActive 执行流程：
1. 检查 event.effectiveLevel，若为 Info 则直接返回
2. 解析动作名称列表：事件级配置 + 等级默认 → 合并
3. 若无匹配配置则返回
4. 依次按名称查 actionTable_，执行对应 callback

executeCleared 执行流程：
1. 查事件级配置的 clearActions 列表
2. 依次按名称查 actionTable_，执行对应 callback
（不自动镜像 active → clear，UI 锁控由前端自行处理）
```

##### 3.1.3.4 行为参数

| 参数 | 约束 | 说明 |
|------|------|------|
| 事件级配置 | 最高优先级 | configureEvent 设置的 active/clear 名称列表 |
| 等级默认 | 附加合并（与事件级合并） | setLevelDefault 设置的默认名称列表 |
| 实例级 | 最低优先级（兜底） | Event 对象的 activeActions/clearActions 字段 |
| effectiveLevel=Info | 不执行联动 | 降级为提示后不触发联动 |

---

#### 3.1.4 告警降级（RX_ALARM_DOWNGRADE）

##### 3.1.4.1 功能需求

系统应支持将指定事件的报警等级降低为更低等级。降级精确到 EventId。降级配置设置后立即生效，产生前或产生后设置均可——新事件产生时自动计算 effectiveLevel，已存在事件的显示也受影响。

降级仅改变 effectiveLevel，影响联动决策（若降为 Info 则不执行联动），但日志和前端通知照常发送。

支持设置降级和取消降级。

##### 3.1.4.2 输入输出

| 接口 | 说明 |
|------|------|
| `setDowngrade(id, newLevel)` | 设置降级，将 EventId 降为指定等级 |
| `removeDowngrade(id)` | 取消降级，恢复原始等级 |
| `getEffectiveLevel(id, originalLevel) → EventLevel` | 查询有效等级（有降级返回降级后，否则返回原始） |

##### 3.1.4.3 行为过程描述

```
设置降级：
1. downgradeMap_[eventId] = newLevel
2. 已存在的活跃事件由前端下次 refresh 时自动反映新等级

取消降级：
1. downgradeMap_.erase(eventId)

getEffectiveLevel 查询：
1. 查 downgradeMap_，存在则返回降级后等级
2. 不存在则返回 originalLevel
```

##### 3.1.4.4 行为参数

| 参数 | 约束 | 说明 |
|------|------|------|
| eventId | 有效的 EventId 字符串 | 精确匹配 |
| newLevel | 1-4，必须 ≤ originalLevel | 降级只能降到更低等级 |
| 生效时机 | 立即 | 不依赖事件是否已产生 |

---

#### 3.1.5 报警屏蔽（RX_ALARM_SHIELD）

##### 3.1.5.1 功能需求

系统应支持对指定事件设置屏蔽。屏蔽精确到 EventId。被屏蔽的事件在产生时不发送前端通知（前端不可见），但联动动作和日志写入照常执行。

支持设置屏蔽、取消屏蔽、查询屏蔽数量。

##### 3.1.5.2 输入输出

| 接口 | 说明 |
|------|------|
| `setShield(id)` | 设置屏蔽 |
| `clearShield(id)` | 取消屏蔽 |
| `isShielded(id) → bool` | 查询是否被屏蔽 |
| `getShieldCount() → int` | 获取当前屏蔽总数 |

##### 3.1.5.3 行为过程描述

```
设置屏蔽：
1. shieldSet_.insert(eventId)
2. 此后该事件产生时跳过 notifyFrontend 步骤

取消屏蔽：
1. shieldSet_.erase(eventId)
2. 恢复前端通知

屏蔽计数：
1. 返回 shieldSet_.size()
```

##### 3.1.5.4 行为参数

| 参数 | 约束 | 说明 |
|------|------|------|
| eventId | 有效的 EventId 字符串 | 精确匹配 |
| 屏蔽影响范围 | 仅影响前端通知 | 联动和日志不受影响 |
| 生效时机 | 立即 | 不依赖事件是否已产生 |

---

#### 3.1.6 系统事件（RX_SYSTEM_EVENT）

##### 3.1.6.1 功能需求

系统应支持后端自行监测产生的事件（如通信断连、磁盘满、CPU 过载等），与下位机状态帧事件使用相同的事件管理管线。

系统事件名必须在 `backend/system_events.cpp` 的预定义列表中注册，业务代码只能使用已注册的名称，不允许任意取名。

系统事件分为两类：
- **纯系统事件**：不与特定下位机关联（如 `disk_full`）
- **关联设备系统事件**：与某下位机相关（如 `comm_lost:3` 表示下位机3通信断连）

##### 3.1.6.2 输入输出

| 接口 | 说明 |
|------|------|
| `triggerSystemEvent(eventName, isActive)` | 纯系统事件入口 |
| `triggerSystemEvent(eventName, protocolID, isActive)` | 关联设备系统事件入口 |
| `findSystemEventDef(name) → SystemEventDef*` | 按名称查找预定义（内部使用） |

##### 3.1.6.3 行为过程描述

```
triggerSystemEvent(eventName, isActive):
1. findSystemEventDef(eventName) 查预定义列表
2. 未找到 → 打印告警并忽略
3. 找到 → 构造 Event (source=System, id=eventName)
4. isActive ? addEvent : clearEvent

triggerSystemEvent(eventName, protocolID, isActive):
1. 同上验证 eventName
2. EventId = "eventName:protocolID"
3. description = "下位机{protocolID}-{定义描述}"
```

##### 3.1.6.4 行为参数

| 参数 | 约束 | 说明 |
|------|------|------|
| eventName | 必须在 system_events.cpp 预定义 | 运行时校验 |
| protocolID | ≥ 0 整数 | 仅关联设备时有效 |
| 预定义事件内容 | 项目方自行增删 | 示例：comm_lost, disk_full 等 |

---

#### 3.1.7 前端 UI（RX_FRONTEND_UI）

##### 3.1.7.1 功能需求

前端应提供以下可视化界面，通过 BackendBridge 与后端交互：

**a. 活跃事件列表（EventListWidget）**
- 7 列表格：事件编号 | 描述 | 时间 | 等级 | 状态 | 降级 | 屏蔽
- 等级按颜色区分：紧急(红)、严重(橙)、一般(黄)、提示(蓝)
- 降级/屏蔽列使用 QCheckBox，勾选即时写入 ConfigManager
- 每 1 秒自动刷新

**b. 报警配置页（AlarmCatalogWidget）**
- 5 列表格：事件编号 | 描述 | 原始等级 | 降级 | 屏蔽
- 降级/屏蔽列使用 QCheckBox
- "应用配置"按钮批量写入，与事件列表页共享 ConfigManager 状态
- Tab 切换时自动刷新

**c. 实时报警日志（AlarmLogWidget）**
- 2 列表格：时间 | 报警内容
- 每 1 秒轮询 `getActiveEvents()` 刷新
- 可嵌入主界面任意位置

**d. 主容器（EventMgrWidget）**
- QWidget 子类，含 QTabWidget 切换页 + 底部屏蔽数量状态栏
- 可嵌入任意父级 Qt 窗口

##### 3.1.7.2 输入输出

| 接口 | 说明 |
|------|------|
| `BackendBridge::getActiveEvents() → QVector<EventEntry>` | 获取活跃事件（含时间戳） |
| `BackendBridge::getCatalog() → QVector<CatalogEntry>` | 获取报警目录及配置状态 |
| `BackendBridge::setDowngrade/setShield/...` | 配置操作 |
| `BackendBridge::triggerAlarm(id, isActive)` | 模拟报警（开发测试用） |

##### 3.1.7.3 行为过程描述

```
前端刷新流程：
1. QTimer 每 1 秒触发 refresh()
2. refresh() 调用 bridge_->getActiveEvents()
3. 清空表格，重新填充所有行
4. 降级/屏蔽复选框读取当前 ConfigManager 状态

Tab 切换同步：
1. EventMgrWidget 监听 QTabWidget::currentChanged
2. 切到事件列表 → eventListPage_->refresh()
3. 切到报警配置 → catalogPage_->loadCatalog()
```

##### 3.1.7.4 行为参数

| 参数 | 约束 | 说明 |
|------|------|------|
| 刷新周期 | 1 秒 | 平衡实时性与 CPU 占用 |
| 表格编辑 | QAbstractItemView::NoEditTriggers | 只读 |
| 行选择 | QAbstractItemView::SelectRows | 整行选中 |

---

### 3.2 外部接口需求（RX_interface）

#### 3.2.1 接口标识和接口图

```
外部调用方（NetworkReceiver / 通信监测 / observe）
    │
    ├── triggerAlarm(pid, fid, field, bool)
    ├── triggerSystemEvent(name, bool)
    ├── triggerSystemEvent(name, pid, bool)
    ├── createAlarm(pid, fid, field, level, desc) → Event
    ├── addEvent(Event)
    ├── clearEvent(pid, fid, field)
    ├── clearEvent(eventId)
    ├── getActiveEvents() → vector<Event>
    └── getAlarmCatalog() → vector<AlarmDef>
            │
            ▼
      事件管理中心
```

#### 3.2.2 ExternalAPI 接口（JK_API）

**接口标识**：`JK_API`

**提供服务**：事件产生、消除、查询的统一门面接口

| 方法签名 | 说明 |
|----------|------|
| `Event createAlarm(int protocolID, int frameID, const string& alarmField, EventLevel level, const string& description)` | 创建 Device 事件值对象 |
| `void addEvent(const Event& event)` | 提交事件产生 |
| `void triggerAlarm(int protocolID, int frameID, const string& alarmField, bool isActive)` | observe 对接：自动查目录后 addEvent/clearEvent |
| `void triggerSystemEvent(const string& eventName, bool isActive)` | 纯系统事件入口 |
| `void triggerSystemEvent(const string& eventName, int protocolID, bool isActive)` | 关联设备系统事件入口 |
| `void clearEvent(int protocolID, int frameID, const string& alarmField)` | Device 事件消除 |
| `void clearEvent(const string& eventId)` | 按 ID 消除（自动解析格式） |
| `vector<Event> getActiveEvents() const` | 获取所有活跃事件 |
| `vector<AlarmDef> getAlarmCatalog() const` | 获取报警目录及配置状态 |

**通信方法**：直接 C++ 函数调用（一体模式）或通过 BackendBridge 桥接

**协议特征**：
- 同步调用，无异步回调
- 参数为值传递或 const 引用
- 异常输入静默处理，不抛异常

---

### 3.3 内部接口需求（rx_in_interface）

#### 3.3.1 EventManager 内部接口（RX_IN_EVENTMGR）

EventManager 是核心事件存储与调度模块，提供以下内部接口：

| 方法 | 说明 |
|------|------|
| `processAddEvent(const Event&)` | 处理告警产生（查重、降级、存储、联动、通知、日志） |
| `processClearEvent(int, int, const string&)` | 处理设备告警消除 |
| `processClearEvent(const EventId&)` | 处理告警消除（按 ID 精准匹配） |
| `findEvent(const EventId&) → const Event*` | 按 ID 查找事件 |
| `getActiveEvents() → vector<Event>` | 获取所有活跃事件 |
| `findEventsByProtocolID(int) → vector<Event>` | 按设备查找事件 |
| `activeCount() → size_t` | 活跃事件数量 |

#### 3.3.2 LinkageEngine 内部接口（RX_IN_LINKAGE）

LinkageEngine 是联动动作执行引擎，提供以下内部接口：

| 方法 | 说明 |
|------|------|
| `registerAction(name, callback)` | 注册一个命名的可执行动作 |
| `configureEvent(eventId, activeNames, clearNames)` | 为指定 EventId 配置联动 |
| `setLevelDefault(level, names)` | 设置等级默认联动动作 |
| `executeActive(const Event&)` | 执行产生侧联动 |
| `executeCleared(const Event&)` | 执行消除侧联动 |
| `clearAll()` | 清除所有配置 |

#### 3.3.3 ConfigManager 内部接口（RX_IN_CONFIG）

ConfigManager 管理降级和屏蔽配置：

| 方法 | 说明 |
|------|------|
| `setDowngrade(id, level)` | 设置降级 |
| `removeDowngrade(id)` | 取消降级 |
| `hasDowngrade(id) → bool` | 是否已降级 |
| `getEffectiveLevel(id, original) → EventLevel` | 获取有效等级 |
| `setShield(id)` | 设置屏蔽 |
| `clearShield(id)` | 取消屏蔽 |
| `isShielded(id) → bool` | 是否被屏蔽 |
| `getShieldCount() → int` | 屏蔽数量 |

---

### 3.4 内部数据需求（rx_in_data）

#### 3.4.1 事件存储（RX_IN_DATA_EVENT）

```
Key: EventId (std::string)
  Device 事件: "protocolID-frameID-alarmField"
  System 事件: "eventName" 或 "eventName:protocolID"

Value: Event {
    EventId     id;
    EventSource source;         // Device / System
    int         protocolID;     // System 事件可为 0
    int         frameID;        // System 事件为 0
    string      alarmField;     // System 事件同 eventName
    string      description;    // 报警描述
    EventLevel  originalLevel;  // 1-4
    EventLevel  effectiveLevel; // 经降级后的实际等级
    EventState  state;          // Active / Cleared
    string      timestamp;      // "YYYY-MM-DD HH:MM:SS"
    vector<string> activeActions;  // 兜底联动列表
    vector<string> clearActions;   // 兜底联动列表
}

容器: std::unordered_map<EventId, Event>  // O(1) 查重
```

#### 3.4.2 配置存储（RX_IN_DATA_CONFIG）

```
// 降级映射
Key: EventId (string)
Value: EventLevel (降级后的等级)
容器: std::unordered_map<EventId, EventLevel>

// 屏蔽集合
容器: std::unordered_set<EventId>

// 联动动作表
Key: string (动作名称)
Value: function<void()> (回调)
容器: std::unordered_map<string, ActionCallback>

// 事件联动配置表
Key: EventId
Value: pair<vector<string>, vector<string>> (activeNames, clearNames)
容器: std::unordered_map<EventId, pair<vector<string>, vector<string>>>

// 等级默认动作表
Key: int (EventLevel 转 int)
Value: vector<string> (动作名称列表)
容器: std::unordered_map<int, vector<string>>
```

---

### 3.5 设计和实现约束（rx_design）

| 编号 | 约束项 | 说明 |
|------|--------|------|
| RX_DESIGN_01 | 运行平台 | 飞腾 FT/2000 + 银河麒麟 V10 |
| RX_DESIGN_02 | 后端语言 | C++11，仅依赖 STL，不依赖 Qt |
| RX_DESIGN_03 | 前端框架 | Qt 5.15.2，C++11 |
| RX_DESIGN_04 | 前后端分离 | 一体模式通过 BackendBridge 桥接；分离模式可切换为 Socket 代理 |
| RX_DESIGN_05 | 并发模型 | 单线程事件循环 |
| RX_DESIGN_06 | 编译标准 | GCC C++11，-Wall -Wextra -Werror |
| RX_DESIGN_07 | 构造函数/析构函数 | 必须显式声明 |
| RX_DESIGN_08 | 注释 | 所有公开接口必须写注释 |
| RX_DESIGN_09 | 文档 | 提交前完善相关文档 |
| RX_DESIGN_10 | 异常输入 | 静默处理，不抛异常，保证系统稳定 |

---

### 3.6 其他需求（rx_other）

#### 3.6.1 非功能性需求

| 编号 | 类别 | 需求描述 |
|------|------|---------|
| RX_OTHER_01 | 性能 | 事件查重 O(1)，200 msg/s 场景 CPU 占用 ≤ 5% |
| RX_OTHER_02 | 可靠性 | 单设备通信中断不影响其他设备事件处理 |
| RX_OTHER_03 | 可扩展性 | 新增联动动作类型仅需添加 Lambda 注册，核心引擎零改动 |
| RX_OTHER_04 | 可维护性 | 联动配置集中在 ActionRegistry，不散落 |
| RX_OTHER_05 | 兼容性 | 前后端通过回调机制解耦，不影响已有接口签名 |

#### 3.6.2 扩展预留

- 联动引擎 `registerAction` 支持后续新增任意动作类型
- 前端 `EventMgrWidget` 是 QWidget 子类，可嵌入任意宿主窗口
- `BackendBridge` 可从一体模式切换为 Socket 代理模式
- 系统事件预定义列表可由项目方按需增删

---

## 4. 注释

- 本文档中 "下位机" 指被上位机监控的物理设备
- "报警" 与 "事件" 在本文档中可互换使用
- EventId 在 Device 事件中遵循 `"数字-数字-字符串"` 格式，System 事件遵循 `"字符串"` 或 `"字符串:数字"` 格式
- 联动动作的 "消除侧" 指事件消除时执行的清理动作（如停联动、恢复常态）
