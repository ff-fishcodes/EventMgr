# 事件管理中心 — 需求规格说明文档

> 文档编号：EventMgr-RS-001
> 版本：v2.0
> 日期：2026-07-08
> v1.0 → v2.0：需求条目细化为每项独立编号，新增前后端架构兼容、fallback 桥接、signal 推送等需求

---

## 1. 范围

### 1.1 标识

- **系统名称**：事件管理中心 (EventMgr)
- **文档编号**：EventMgr-RS-001
- **适用项目**：上位机事件管理子系统

### 1.2 概述

事件管理中心是上位机系统的核心子系统，负责接收多台下位机的健康状态信息，处理后端自行监测的系统事件，生成告警、执行联动管控动作、实时呈现事件状态。

**核心能力**：告警产生与消除、事件联动、降级与屏蔽配置、系统事件支持、前端实时展示

**运行环境**：飞腾 FT/2000 + 银河麒麟 V10，后端 C++11（不依赖 Qt），前端 Qt 5.15.2

**规模假设**：下位机 5-20 台，状态帧总数 ≤ 100，上报频率 ≥ 1 Hz

---

## 2. 引用文档

| 序号 | 文档名称 | 路径 |
|------|---------|------|
| 1 | 设计方案 v5 | docs/superpowers/specs/2026-06-17-event-mgr-design.md |
| 2 | 需求讨论记录 | docs/superpowers/specs/2026-06-17-requirement-discussion-record.md |
| 3 | ActionRegistry 设计 | docs/superpowers/specs/2026-06-26-action-registry-design.md |
| 4 | 软件设计说明 | docs/superpowers/specs/2026-07-06-software-design-spec.md |

---

## 3. 需求

### 3.1 能力需求（RX_BASIC_ABILITY）

---

#### 3.1.1 事件编号（RX_ALARM_EVENT_ID）

##### 3.1.1.1 EventId 格式定义（RX_ALARM_EVENT_ID_01）

系统应为每个告警事件生成全局唯一的标识符 EventId。

**Device 事件**（下位机状态帧上报）：

格式：`"{protocolID}-{frameID}-{alarmField}"`

| 部分 | 类型 | 说明 | 示例 |
|------|------|------|------|
| protocolID | int | 下位机通信标识 | `1` |
| frameID | int | 状态帧标识 | `3` |
| alarmField | string | 报警字段名 | `temp_high` |

完整示例：`"1-3-temp_high"` 表示下位机 1、状态帧 3 中的温度过高告警。

**System 事件**（后端自行监测）：

| 类型 | 格式 | 示例 |
|------|------|------|
| 纯系统事件 | `"{eventName}"` | `"disk_full"` |
| 关联设备系统事件 | `"{eventName}:{protocolID}"` | `"comm_lost:3"` |

**唯一性约束**：EventId 在全局范围内唯一，同一时刻只有一个活跃实例。

##### 3.1.1.2 重复上报抑制（RX_ALARM_EVENT_ID_02）

同一 EventId 的事件已处于活跃状态时，再次收到该 EventId 的产生请求必须**静默忽略**：

- 不创建新 Event 对象
- 不重复存入 activeEvents_
- 不重复触发联动动作
- 不重复发送前端通知
- 不重复写日志

消除不存在的事件也必须**静默忽略**：不报错、不抛异常、不影响系统运行。

##### 3.1.1.3 事件来源区分（RX_ALARM_EVENT_ID_03）

Event 结构体必须包含 `EventSource` 枚举字段，区分两类来源：

| 值 | 含义 | ID 格式 |
|----|------|--------|
| `Device` | 下位机状态帧上报 | `P-F-A` |
| `System` | 后端自行监测 | `name` 或 `name:P` |

该字段用于：
- 前端可按来源过滤展示
- 系统事件校验逻辑与 Device 事件不同
- 后续扩展新来源时不影响现有格式

##### 3.1.1.4 事件等级定义（RX_ALARM_EVENT_ID_04）

| 枚举值 | 数值 | 含义 | 联动行为 |
|--------|------|------|---------|
| `Emergency` | 1 | 紧急故障 | 执行联动 |
| `Serious` | 2 | 严重故障 | 执行联动 |
| `General` | 3 | 一般故障 | 执行联动 |
| `Info` | 4 | 提示 | **不执行联动** |

effectiveLevel 为 Info 时跳过联动，但日志和通知照常发送。

##### 3.1.1.5 事件时间戳（RX_ALARM_EVENT_ID_05）

Event 结构体必须包含 `timestamp` 字段，格式 `"YYYY-MM-DD HH:MM:SS"`（精确到秒）。

- 在 `EventManager::processAddEvent` 中自动生成，调用方不需要传入
- 使用系统本地时间 `std::localtime`
- 前端事件列表和实时日志控件均展示此时间

##### 3.1.1.6 Event 结构体完整性（RX_ALARM_EVENT_ID_06）

Event 值对象必须包含以下全部字段：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| id | EventId | 是 | 唯一标识 |
| source | EventSource | 是 | 来源（默认 Device） |
| protocolID | int | 是 | 下位机标识（System 可为 0） |
| frameID | int | 是 | 状态帧标识（System 为 0） |
| alarmField | string | 是 | 报警字段名（System 同 eventName） |
| description | string | 是 | 报警描述文本 |
| originalLevel | EventLevel | 是 | 原始等级（1-4） |
| effectiveLevel | EventLevel | 是 | 经降级计算后的实际等级 |
| state | EventState | 是 | Active / Cleared |
| timestamp | string | 是 | 产生时间 |
| activeActions | vector\<string\> | 否 | 兜底联动名称列表 |
| clearActions | vector\<string\> | 否 | 兜底消除联动名称列表 |

##### 3.1.1.7 AlarmDef 报警定义（RX_ALARM_EVENT_ID_07）

AlarmDef 结构体表示报警目录中的一条定义：

| 字段 | 类型 | 说明 |
|------|------|------|
| id | EventId | 事件编号 |
| description | string | 报警描述 |
| originalLevel | EventLevel | 原始等级 |
| downgraded | bool | 是否已降级 |
| downgradeTo | EventLevel | 降级目标等级 |
| shielded | bool | 是否已屏蔽 |

---

#### 3.1.2 架构兼容性（RX_ARCH_COMPAT）

##### 3.1.2.1 前后端分离架构（RX_ARCH_COMPAT_01）

系统必须支持前后端分离部署：

- 后端纯 C++11，**不依赖 Qt 库**
- 前端 Qt 5.15.2
- 前后端通过 JSON 消息通信（Socket 或本地管道）
- EventManager 通过 NotifyCallback 推送事件变更 JSON 到前端
- 桩模块（SocketServer/LogWriter/CmdSender/BuzzerControl）在集成时替换为真实实现

##### 3.1.2.2 前后端一体架构（RX_ARCH_COMPAT_02）

系统必须**同时**支持前后端不分离的本机部署：

- 后端代码**不做任何修改**
- 通过 BackendBridge（QObject）直接持有后端实例
- EventManager 的 NotifyCallback 注入为 emit Qt signal 的 Lambda
- 单进程内前后端零拷贝通信

##### 3.1.2.3 架构切换（RX_ARCH_COMPAT_03）

一体模式与分离模式的切换**不应要求修改后端核心代码**：

- 切换点仅在 EventManager 构造时注入的 NotifyCallback
- 一体模式：回调 → emit eventsChanged() signal
- 分离模式：回调 → SocketServer::notifyFrontend(json)
- 不传回调时默认走 SocketServer（向后兼容）

##### 3.1.2.4 桩模块替换（RX_ARCH_COMPAT_04）

当前桩模块在集成时需替换为真实实现，接口不变：

| 桩模块 | 接口 | 集成时对接 |
|--------|------|-----------|
| SocketServer | `notifyFrontend(json)` | 项目实际 Socket 通信层 |
| LogWriter | `write(msg)` | 项目日志系统 |
| CmdSender | `send(protocolID, target, param)` | 下位机指令发送模块 |
| BuzzerControl | `play(target, param)` | 蜂鸣器硬件 |
| AlarmCatalog | `getAllDefinitions()` | 项目配置模块 |

替换仅需修改 .cpp 实现，头文件接口不变。

---

#### 3.1.3 告警产生（RX_ALARM_GENERATE）

##### 3.1.3.1 triggerAlarm 接口（RX_ALARM_GENERATE_01）

`triggerAlarm(protocolID, frameID, alarmField, isActive)` 是 observe 组件的对接入口：

- `isActive=true`：自动查 AlarmCatalog 获取等级和描述 → createAlarm → addEvent
- `isActive=false`：调用 clearEvent 消除
- AlarmCatalog 中找不到匹配定义时，使用默认值（Info 等级 + alarmField 作描述）兜底
- 调用方不需要知道等级和描述，只传设备标识 + 报警字段

##### 3.1.3.2 createAlarm 工厂（RX_ALARM_GENERATE_02）

`createAlarm(protocolID, frameID, alarmField, level, description)` 创建 Device 事件值对象：

- 自动生成 EventId = `"{protocolID}-{frameID}-{alarmField}"`
- 设置 source = Device
- effectiveLevel 初始 = originalLevel（后续 addEvent 时由 ConfigManager 修正）
- 不触发任何业务逻辑（纯工厂方法）

##### 3.1.3.3 addEvent 提交（RX_ALARM_GENERATE_03）

`addEvent(event)` 提交事件到 EventManager 管线：

- 调 `EventManager::processAddEvent`，完成查重、降级计算、存储、联动、通知、日志全部流程
- 入参为 Event 值对象，接口不随 Event 字段变更而改动

##### 3.1.3.4 processAddEvent 流程（RX_ALARM_GENERATE_04）

EventManager 处理流程必须严格按以下顺序：

```
1. 查重：若 activeEvents_.find(event.id) 已存在，直接返回（静默忽略）
2. 计算有效等级：effectiveLevel = configMgr_.getEffectiveLevel(id, originalLevel)
3. 生成时间戳：strftime("%Y-%m-%d %H:%M:%S")
4. 存储：activeEvents_[id] = event
5. 联动：若 effectiveLevel ≠ Info，调用 linkageEng_.executeActive(event)
6. 通知：若 configMgr_.isShielded(id) == false，调用 notifyFrontend(event, true)
7. 日志：writeLog(event, "发生")
```

**关键约束**：
- 屏蔽只影响前端通知（步骤 6），不影响联动（步骤 5）和日志（步骤 7）
- 降级影响联动决策（effectiveLevel 参与步骤 5 判断）
- 重复事件在步骤 1 返回，不执行后续任何步骤

##### 3.1.3.5 日志写入（RX_ALARM_GENERATE_05）

事件产生时必须写入日志，格式为：
```
"下位机{protocolID}-{description}-发生"
```
日志通过 LogWriter 桩写入，集成时替换为项目实际日志系统。

---

#### 3.1.4 告警消除（RX_ALARM_CLEAR）

##### 3.1.4.1 clearEvent 三字段消除（RX_ALARM_CLEAR_01）

`clearEvent(protocolID, frameID, alarmField)` 按 Device 事件三字段消除：

- 内部构造 EventId = `"{protocolID}-{frameID}-{alarmField}"`
- 调 `EventManager::processClearEvent(int, int, string)`

##### 3.1.4.2 clearEvent 按 ID 消除（RX_ALARM_CLEAR_02）

`clearEvent(eventId)` 按 EventId 直接消除：

- 含两个 '-' 时按 Device 格式解析：提取 pid/fid/field → processClearEvent(pid, fid, field)
- 否则按 System 事件处理：processClearEvent(eventId) 精准 ID 匹配
- 该重载使得 `triggerSystemEvent(name, false)` 等消除路径无需解析 ID 格式

##### 3.1.4.3 processClearEvent 流程（RX_ALARM_CLEAR_03）

消除流程严格按以下顺序：

```
1. 查找：activeEvents_.find(id)，不存在则直接返回（静默忽略）
2. 标记：event.state = Cleared
3. 消除联动：linkageEng_.executeCleared(event)
4. 通知：notifyFrontend(event, false)  // alarm_cleared
5. 日志：writeLog(event, "消除")
6. 移除：activeEvents_.erase(id)
```

##### 3.1.4.4 消除日志（RX_ALARM_CLEAR_04）

消除日志格式为：
```
"下位机{protocolID}-{description}-消除"
```

---

#### 3.1.5 事件联动（RX_EVENT_LINKAGE）

##### 3.1.5.1 动作注册（RX_EVENT_LINKAGE_01）

`LinkageEngine::registerAction(name, callback)` 注册一个命名的可执行动作：

- name 为任意字符串（如 `"cooler_stop"`、`"boiler_stop"`）
- callback 为 `std::function<void()>` Lambda，闭包内捕获所需的参数（目标设备、指令等）
- 同名注册后者覆盖前者
- 仅在系统启动时调用，运行时不再注册

##### 3.1.5.2 事件联动配置（RX_EVENT_LINKAGE_02）

`LinkageEngine::configureEvent(eventId, activeNames, clearNames)` 为指定 EventId 绑定动作列表：

- activeNames：事件产生时执行的动作名称列表
- clearNames：事件消除时执行的动作名称列表
- clearNames 不自动从 activeNames 推导（不自动镜像），UI 锁控由宿主处理
- 每个名称必须在 actionTable_ 中已注册，否则该名称被跳过（不报错）

##### 3.1.5.3 等级默认联动（RX_EVENT_LINKAGE_03）

`LinkageEngine::setLevelDefault(level, names)` 为指定等级设置默认联动动作：

- 所有该等级的事件产生时自动附加这些动作
- 与事件级配置**合并执行**（非覆盖）

##### 3.1.5.4 联动优先级合并规则（RX_EVENT_LINKAGE_04）

运行时 resolveActiveNames 按以下优先级合并：

| 优先级 | 来源 | 配置方式 |
|--------|------|---------|
| 1（最高） | 事件级 | configureEvent 设置的 activeNames |
| 2 | 等级级 | setLevelDefault 设置的默认动作 |
| 3（最低） | 实例级 | Event 对象的 activeActions 字段（兜底） |

合并方式：事件级 + 等级级 → 追加拼接。若合并结果为空，fallback 到实例级。

resolveClearNames 仅使用事件级 clearNames，不合并等级默认，不 fallback 到实例级。

##### 3.1.5.5 Info 等级跳过联动（RX_EVENT_LINKAGE_05）

`executeActive` 和 `executeCleared` 在 `event.effectiveLevel == EventLevel::Info` 时必须直接返回，不执行任何动作，也不调用 fallback。

##### 3.1.5.6 Fallback 桥接（RX_EVENT_LINKAGE_06）

LinkageEngine 必须支持 `setFallback(callback)` 注入回调，签名：
```cpp
void callback(const std::string& eventId, bool isActive)
```

- executeActive 末尾调用：`fallback_(event.id, true)`
- executeCleared 末尾调用：`fallback_(event.id, false)`
- Fallback 在 **executeNames 执行之后**调用，不影响已注册动作的执行
- 若未设置 fallback（callback 为空），不调用

##### 3.1.5.7 BackendBridge 桥接信号（RX_EVENT_LINKAGE_07）

BackendBridge 必须将 fallback 转换为 Qt 信号：

```cpp
// 信号声明
void linkageAction(const QString& eventId, bool isActive);
```

- BackendBridge::initialize() 中注入 setFallback Lambda
- Lambda 内部 `emit linkageAction(eventId, isActive)`
- 宿主项目 connect 此信号，按 eventId 自行处理 UI 锁控（lock/unlock）

##### 3.1.5.8 ActionRegistry 集中注册（RX_EVENT_LINKAGE_08）

所有联动动作注册和事件联动配置必须在 `ActionRegistry::setup(LinkageEngine&)` 中集中完成：

- 系统启动时一次性调用
- 注册和配置不散落在业务代码中
- 仅注册后端管控动作（CmdSender/Buzzer），不注册 UI 动作
- UI 锁控通过 fallback 桥接到宿主项目

##### 3.1.5.9 联动引擎存储结构（RX_EVENT_LINKAGE_09）

LinkageEngine 内部使用以下数据结构：

| 存储 | Key | Value | 说明 |
|------|-----|-------|------|
| actionTable_ | string（动作名） | ActionCallback | O(1) 查找回调 |
| eventConfig_ | EventId | pair<activeNames, clearNames> | 事件级配置 |
| levelDefaults_ | int（等级值） | vector\<string\> | 等级默认动作 |
| fallback_ | — | FallbackCallback | 桥接回调（可空） |

---

#### 3.1.6 告警降级（RX_ALARM_DOWNGRADE）

##### 3.1.6.1 降级设置（RX_ALARM_DOWNGRADE_01）

`ConfigManager::setDowngrade(eventId, newLevel)` 将指定事件降级：

- eventId 精确匹配
- newLevel 为降级后的等级（1-4）
- 设置后立即生效，不依赖事件是否已产生
- 重复设置会覆盖之前的降级配置

##### 3.1.6.2 降级取消（RX_ALARM_DOWNGRADE_02）

`ConfigManager::removeDowngrade(eventId)` 取消降级配置：

- 从 downgradeMap_ 中移除
- 取消后事件恢复 originalLevel
- 对已存在的活跃事件，前端下次 refresh 时自动反映恢复后的等级

##### 3.1.6.3 有效等级查询（RX_ALARM_DOWNGRADE_03）

`ConfigManager::getEffectiveLevel(eventId, originalLevel)`：

- 查 downgradeMap_，若存在返回降级后等级
- 不存在返回 originalLevel
- 该查询在 processAddEvent 中调用，确保 effectiveLevel 正确

##### 3.1.6.4 降级影响范围（RX_ALARM_DOWNGRADE_04）

降级仅改变 effectiveLevel：

| 受影响 | 不受影响 |
|--------|---------|
| 联动决策（Info 时跳过联动） | 日志写入（始终执行） |
| 前端等级显示 | 前端通知（始终推送，除非被屏蔽） |

##### 3.1.6.5 降级配置不持久化（RX_ALARM_DOWNGRADE_05）

降级配置存储在内存中（downgradeMap_），软件重启后清空恢复原始状态。操作员必须在每次启动后手动重新设置降级。此设计是刻意的安全策略。

---

#### 3.1.7 报警屏蔽（RX_ALARM_SHIELD）

##### 3.1.7.1 屏蔽设置（RX_ALARM_SHIELD_01）

`ConfigManager::setShield(eventId)` 将指定事件标记为屏蔽：

- eventId 精确匹配
- 设置后立即生效
- 被屏蔽的事件的 processAddEvent 流程中，步骤 6（通知前端）被跳过

##### 3.1.7.2 屏蔽取消（RX_ALARM_SHIELD_02）

`ConfigManager::clearShield(eventId)` 取消屏蔽：

- 从 shieldSet_ 中移除
- 后续事件产生时恢复前端通知

##### 3.1.7.3 屏蔽查询（RX_ALARM_SHIELD_03）

`ConfigManager::isShielded(eventId) → bool`：查询指定事件是否被屏蔽。

##### 3.1.7.4 屏蔽计数（RX_ALARM_SHIELD_04）

`ConfigManager::getShieldCount() → int`：返回 shieldSet_.size()，供前端状态栏持续显示。

##### 3.1.7.5 屏蔽影响范围（RX_ALARM_SHIELD_05）

屏蔽仅影响前端通知：

| 受影响 | 不受影响 |
|--------|---------|
| 前端通知（alarm_active 不推送） | 联动动作（正常执行） |
| 前端列表可见性 | 日志写入（正常写入） |

##### 3.1.7.6 屏蔽配置不持久化（RX_ALARM_SHIELD_06）

与降级相同，屏蔽配置存储在内存中，软件重启后清空。操作员必须在每次启动后手动重新设置屏蔽。

---

#### 3.1.8 系统事件（RX_SYSTEM_EVENT）

##### 3.1.8.1 系统事件预定义列表（RX_SYSTEM_EVENT_01）

系统事件名必须在 `backend/system_events.cpp` 的 `kSystemEvents` 列表中集中预定义：

- 每个条目包含 {name, description, level}
- `getSystemEventDefs()` 返回完整列表
- `findSystemEventDef(name)` 按名称查找，未找到返回 NULL

列表内容由项目方自行增删，当前示例：
```cpp
{"comm_lost",    "下位机通信断连",  EventLevel::Emergency},
{"comm_restore", "下位机通信恢复",  EventLevel::Info},
{"disk_full",    "磁盘空间不足",    EventLevel::Serious},
{"cpu_overload", "CPU 过载",        EventLevel::Serious},
{"service_error","关键服务异常",    EventLevel::Emergency},
```

##### 3.1.8.2 triggerSystemEvent 纯系统事件（RX_SYSTEM_EVENT_02）

`triggerSystemEvent(eventName, isActive)` 触发不与特定下位机关联的系统事件：

- 校验 eventName 是否在预定义列表中，不在则打印警告并忽略
- isActive=true：构造 Event {source=System, id=eventName, ...} → addEvent
- isActive=false：clearEvent(eventName)
- description/level 从预定义列表获取

##### 3.1.8.3 triggerSystemEvent 关联设备（RX_SYSTEM_EVENT_03）

`triggerSystemEvent(eventName, protocolID, isActive)` 触发与某下位机关联的系统事件：

- EventId = `"{eventName}:{protocolID}"`（如 `"comm_lost:3"`）
- description = `"下位机{protocolID}-{预定义描述}"`
- 其他流程同上

##### 3.1.8.4 系统事件与联动（RX_SYSTEM_EVENT_04）

系统事件与 Device 事件共享同一联动管线：

- 在 ActionRegistry 中可通过 `configureEvent("comm_lost", {...}, {...})` 为系统事件配置联动
- 也可通过 `setLevelDefault` 让系统事件按等级触发默认联动
- 系统事件的 fallback 回调同样在 executeActive/executeCleared 末尾触发

---

#### 3.1.9 前端 UI（RX_FRONTEND_UI）

##### 3.1.9.1 活跃事件列表控件（RX_FRONTEND_UI_01）

EventListWidget 提供活跃事件实时列表，要求：

| 项目 | 规格 |
|------|------|
| 列定义 | 编号 | 描述 | 时间 | 等级 | 状态 | 降级 | 屏蔽（7 列） |
| 时间格式 | `YYYY-MM-DD HH:MM:SS` |
| 等级着色 | 紧急=红色、严重=橙色、一般=黄色、提示=蓝色 |
| 降级列 | QCheckBox，勾选即时调用 bridge_->setDowngrade()，取消勾选调用 removeDowngrade() |
| 屏蔽列 | QCheckBox，勾选即时调用 bridge_->setShield()，取消勾选调用 clearShield() |
| 编辑 | QAbstractItemView::NoEditTriggers（只读） |
| 选择 | QAbstractItemView::SelectRows（整行选中） |

##### 3.1.9.2 报警配置页控件（RX_FRONTEND_UI_02）

AlarmCatalogWidget 提供报警定义配置表：

| 项目 | 规格 |
|------|------|
| 列定义 | 编号 | 描述 | 原始等级 | 降级 | 屏蔽（5 列） |
| 降级/屏蔽列 | QCheckBox，用户勾选后需点击"应用配置"按钮批量写入 |
| 按钮 | "应用配置"（遍历所有行批量调 setDowngrade/setShield/removeDowngrade/clearShield）、"刷新"（重载目录） |
| 与事件列表同步 | 两页共享同一 ConfigManager，切换 Tab 时自动刷新 |

##### 3.1.9.3 实时报警日志控件（RX_FRONTEND_UI_03）

AlarmLogWidget 提供可嵌入主界面的实时报警日志：

| 项目 | 规格 |
|------|------|
| 列定义 | 时间 | 报警内容（2 列） |
| 描述着色 | 按等级着色（同事件列表） |
| 独立性 | 不依赖 EventMgrWidget，可嵌入任意父级 QWidget |
| 数据来源 | bridge_->getActiveEvents() |

##### 3.1.9.4 主容器控件（RX_FRONTEND_UI_04）

EventMgrWidget 为前端主容器：

| 项目 | 规格 |
|------|------|
| 基类 | QWidget（可嵌入任意父级窗口） |
| 内含 | QTabWidget（事件列表 + 报警配置）+ 底部 QLabel 屏蔽状态栏 |
| Tab 切换 | 监听 QTabWidget::currentChanged，切换时自动刷新目标页面 |
| 状态栏 | 每 1 秒更新 `"当前屏蔽: N 个报警"` |

##### 3.1.9.5 Signal 推送刷新（RX_FRONTEND_UI_05）

BackendBridge 必须提供 `eventsChanged()` 信号：

- EventManager 的 NotifyCallback 注入为 emit eventsChanged() 的 Lambda
- 事件产生或消除后瞬间触发（< 1ms）
- EventListWidget 和 AlarmLogWidget 连接此信号即时刷新
- 降低刷新延迟从"最多 1 秒"到"几乎实时"

##### 3.1.9.6 Timer 兜底刷新（RX_FRONTEND_UI_06）

1 秒 QTimer 周期刷新保留作为兜底：

- 防止 signal 丢失或异常导致界面停滞
- 两个刷新机制共存，互不冲突
- 屏蔽状态栏的更新也通过 1 秒定时器保障

##### 3.1.9.7 模拟报警功能（RX_FRONTEND_UI_07）

EventListWidget 底部提供 QComboBox（选择报警定义）+ QPushButton "模拟报警"：

- 点击后调 bridge_->triggerAlarm(id, true)
- 用于开发调试阶段模拟事件产生
- 仅产生不自动消除

##### 3.1.9.8 Down/upcast 控件嵌入性（RX_FRONTEND_UI_08）

所有前端控件均为 QWidget 子类，可嵌入任意父级：

- EventMgrWidget：含有完整的事件管理两个 Tab 页，一行 `new EventMgrWidget(parent)` 即可嵌入
- AlarmLogWidget：独立的实时报警日志，一行 `new AlarmLogWidget(bridge, parent)` 即可嵌入

##### 3.1.9.9 BackendBridge 桥接层（RX_FRONTEND_UI_09）

BackendBridge 为 QObject，是前端与后端的唯一桥接点：

| 职责 | 说明 |
|------|------|
| 持有后端实例 | ConfigManager*, LinkageEngine*, EventManager*, ExternalAPI* |
| 初始化 | `initialize()` 创建后端实例 + 调用 ActionRegistry::setup() + 注入 fallback |
| 信号 | eventsChanged()、linkageAction(eventId, isActive) |
| 数据转换 | 后端 std::string → Qt QString，std::vector → QVector |

---

#### 3.1.10 前端通知机制（RX_FRONTEND_NOTIFY）

##### 3.1.10.1 NotifyCallback 注入（RX_FRONTEND_NOTIFY_01）

EventManager 构造函数接受可选的 `NotifyCallback`：

- 签名：`void(const std::string& json)`
- 一体模式：BackendBridge 注入 emit eventsChanged() 的 Lambda
- 分离模式：注入 SocketServer::notifyFrontend 的函数指针
- 不传回调时默认走 SocketServer（向后兼容）

##### 3.1.10.2 前端通知 JSON 格式（RX_FRONTEND_NOTIFY_02）

Notyfi Frontend 发送的 JSON 消息格式：

**事件产生**：
```json
{
    "type": "alarm_active",
    "protocolID": 1,
    "frameID": 3,
    "alarmField": "temp_high",
    "description": "下位机1-温度过高",
    "level": 1
}
```

**事件消除**：
```json
{
    "type": "alarm_cleared",
    "protocolID": 1,
    "frameID": 3,
    "alarmField": "temp_high",
    "description": "下位机1-温度过高",
    "level": 1
}
```

##### 3.1.10.3 通知抑制（RX_FRONTEND_NOTIFY_03）

当事件被屏蔽时，notifyFrontend 步骤被跳过，前端不显示该事件。

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

ExternalAPI 是本模块的唯一对外门面，所有外部调用均通过此类。

| 方法 | 参数 | 返回 | 说明 |
|------|------|------|------|
| `createAlarm` | int pid, int fid, const string& field, EventLevel level, const string& desc | Event | 创建 Device 事件值对象，自动生成 EventId |
| `addEvent` | const Event& | void | 提交事件到 EventManager 管线 |
| `triggerAlarm` | int pid, int fid, const string& field, bool isActive | void | observe 对接：自动查目录后 addEvent/clearEvent |
| `triggerSystemEvent` | const string& name, bool isActive | void | 纯系统事件入口 |
| `triggerSystemEvent` | const string& name, int pid, bool isActive | void | 关联设备系统事件入口 |
| `clearEvent` | int pid, int fid, const string& field | void | Device 事件消除 |
| `clearEvent` | const string& eventId | void | 按 ID 消除（自动解析格式） |
| `getActiveEvents` | — | vector\<Event\> | 获取所有活跃事件 |
| `getAlarmCatalog` | — | vector\<AlarmDef\> | 获取报警目录及降级/屏蔽状态 |

**通信方法**：直接 C++ 函数调用（一体模式），分离模式下通过 BackendBridge 或 Socket 桥接

**异常处理**：所有方法不抛异常；无效输入（如 eventName 不在预定义列表）静默忽略

**同步性**：所有方法同步返回，无异步回调

---

### 3.3 内部接口需求（rx_in_interface）

#### 3.3.1 EventManager 内部接口（RX_IN_EVENTMGR）

| 方法 | 说明 |
|------|------|
| `processAddEvent(const Event&)` | 查重→降级→时间戳→存储→联动→通知→日志 |
| `processClearEvent(int, int, const string&)` | 构造 EventId → 按 ID 查找并消除（Device 事件） |
| `processClearEvent(const EventId&)` | 按 EventId 精准查找并消除（System 事件用） |
| `findEvent(const EventId&) → const Event*` | 按 ID 查找活跃事件，不存在返回 NULL |
| `getActiveEvents() → vector<Event>` | 获取所有活跃事件 |
| `findEventsByProtocolID(int) → vector<Event>` | 按 protocolID 查找所有活跃事件 |
| `activeCount() → size_t` | 活跃事件数量 |
| `makeEventId(int, int, const string&) → EventId` | 构造 "P-F-A" 格式 EventId（静态方法） |

#### 3.3.2 LinkageEngine 内部接口（RX_IN_LINKAGE）

| 方法 | 说明 |
|------|------|
| `registerAction(name, callback)` | 注册命名的可执行动作 |
| `setFallback(callback)` | 设置 per-execute 的 fallback 回调 |
| `configureEvent(eventId, active, clear)` | 为指定 EventId 配置联动名称列表 |
| `setLevelDefault(level, names)` | 为指定等级设置默认联动名称列表 |
| `executeActive(const Event&)` | 执行产生侧联动动作 + 末尾调 fallback |
| `executeCleared(const Event&)` | 执行消除侧联动动作 + 末尾调 fallback |
| `clearAll()` | 清除所有配置和注册 |

#### 3.3.3 ConfigManager 内部接口（RX_IN_CONFIG）

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

#### 3.4.1 事件存储（RX_IN_DATA_STORE）

```
活跃事件表（核心）：
  容器: std::unordered_map<EventId, Event>
  操作: O(1) 查找/插入/删除
  生命周期: 事件产生时插入，消除时 erase
  不持久化: 重启后从下位机周期上报重建
  容量上限: ≤ 100（受规模假设约束）
```

#### 3.4.2 配置存储（RX_IN_DATA_CONFIG）

```
降级映射：std::unordered_map<EventId, EventLevel>
屏蔽集合：std::unordered_set<EventId>
联动动作表：std::unordered_map<string, ActionCallback>
事件联动配置：std::unordered_map<EventId, pair<vector<string>, vector<string>>>
等级默认：std::unordered_map<int, vector<string>>
```
所有配置均在内存中，不持久化。

---

### 3.5 设计和实现约束（rx_design）

| 编号 | 约束项 | 说明 |
|------|--------|------|
| RX_DESIGN_01 | 平台 | 飞腾 FT/2000 + 银河麒麟 V10 |
| RX_DESIGN_02 | 后端语言 | C++11，仅依赖 STL，不依赖 Qt |
| RX_DESIGN_03 | 前端框架 | Qt 5.15.2，C++11 |
| RX_DESIGN_04 | 前后端架构 | 一体模式通过 BackendBridge；分离模式切换为 Socket（架构切换不改后端核心） |
| RX_DESIGN_05 | 并发模型 | 单线程事件循环，无锁设计 |
| RX_DESIGN_06 | 编译标准 | GCC C++11，-Wall -Wextra -Werror |
| RX_DESIGN_07 | 构造函数/析构函数 | 必须显式声明 |
| RX_DESIGN_08 | 注释规范 | 所有公开接口必须写注释 |
| RX_DESIGN_09 | 文档提交 | 提交 git 前完善相关文档 |
| RX_DESIGN_10 | 异常输入 | 静默处理，不抛异常 |
| RX_DESIGN_11 | 第三方库 | 后端无第三方库依赖；前端 Qt 5.15.2 |

---

### 3.6 其他需求（rx_other）

#### 3.6.1 非功能性需求

| 编号 | 类别 | 描述 |
|------|------|------|
| RX_OTHER_01 | 性能 | 事件查重 O(1)；200 msg/s 场景 CPU ≤ 5%；单次 processAddEvent ≤ 1ms |
| RX_OTHER_02 | 可靠性 | 单下位机通信中断不影响其他设备事件；异常输入不崩溃 |
| RX_OTHER_03 | 可扩展性 | 新增联动类型仅需添加 Lambda + registerAction；新增前端页面向 QTabWidget 插页 |
| RX_OTHER_04 | 可维护性 | 联动配置集中在 ActionRegistry；前后端通过回调解耦 |
| RX_OTHER_05 | 可移植性 | 后端纯 C++11，无平台特定代码 |
| RX_OTHER_06 | 安全性 | 降级和屏蔽必须由操作员手动重置，重启后恢复原始状态 |

#### 3.6.2 测试需求

| 场景 | 验证点 |
|------|--------|
| 正常产生 | addEvent → 查重通过 → 联动执行 → 通知推送 → 日志写入 |
| 重复上报 | 同 EventId 再次 addEvent → 静默忽略 |
| 正常消除 | clearEvent → 消除联动 → 通知 → 日志 → erase |
| 消除不存在事件 | clearEvent 不存在的 ID → 静默忽略 |
| 降级生效 | setDowngrade 后 effectiveLevel 正确变更 |
| 降级 → Info | effectiveLevel = Info 时联动不执行 |
| 屏蔽生效 | setShield 后前端通知被抑制 |
| 系统事件 | triggerSystemEvent 产生和消除均正常走管线 |
| 系统事件非法名 | 未在预定义列表的名称 → 静默忽略 |
| Signal 刷新 | 事件产生后 eventsChanged() 被 emit，前端 refresh 被触发 |
| Tab 切换同步 | 切换 Tab 后目标页面数据为最新 |

---

## 4. 注释

- "下位机" 指被上位机监控的物理设备
- "报警" 与 "事件" 可互换使用
- "消除侧" 指事件消除时执行的清理动作
- 桩模块仅供早期开发，集成时替换为真实实现
- 文档中 `RX_` 编号体系与设计文档中 `CSC_`/`CSU_` 编号形成追踪关系
