# 事件管理中心 — 软件设计说明文档

> 文档编号：EventMgr-DS-001
> 版本：v1.0
> 日期：2026-07-06
> 说明：本文档按照项目软件设计说明文档模板格式编写，不覆盖已有文档

---

## 1. 文档标识

- **项目名称**：事件管理中心 (EventMgr)
- **文档编号**：EventMgr-DS-001
- **文档类型**：软件设计说明 (Software Design Description)
- **适用标准**：项目 CSCI/CSC/CSU 体系

---

## 2. 引用文档

| 序号 | 文档名称 | 文档编号 | 版本 |
|------|---------|----------|------|
| 1 | 需求规格说明文档 | EventMgr-RS-001 | v1.0 |
| 2 | 设计方案 | docs/superpowers/specs/2026-06-17-event-mgr-design.md | v5 |
| 3 | 需求讨论记录 | docs/superpowers/specs/2026-06-17-requirement-discussion-record.md | — |
| 4 | ActionRegistry 设计 | docs/superpowers/specs/2026-06-26-action-registry-design.md | — |
| 5 | 架构图 | docs/superpowers/specs/2026-06-25-architecture-diagrams.md | — |

---

## 3. CSCI级设计决策

### 决策 1：前后端分离 + 桥接层一体模式

**决策**：后端纯 C++11（不依赖 Qt），前端 Qt 5.15.2。当前采用一体模式，通过 `BackendBridge` 直接持有后端实例。预留分离模式——BackendBridge 可替换为 Socket 代理，接口不变。

**理由**：满足 C++11 后端约束；一体模式降低开发阶段的线程/通信复杂度；桥接层保证两种模式切换时外部接口不变。

### 决策 2：字符串名称注册 + Lambda 回调的联动模型

**决策**：联动动作以字符串名称为标识，通过 `registerAction(name, callback)` 注册 Lambda 闭包。事件联动的 active/clear 动作列表使用字符串名称向量。

**理由**：Lambda 闭包捕获各自所需的参数（如目标设备、指令等），避免了 `LinkageAction` 结构体中 target/param 等通用字段的语义匹配问题。字符串名称使配置表可读、可维护。

### 决策 3：ActionRegistry 集中注册

**决策**：所有联动动作注册和事件联动配置集中在 `ActionRegistry::setup()` 中完成，业务代码不散落注册逻辑。

**理由**：满足"启动时一处配置"的设计目标。新增设备控制单例时只需在 setup() 中加一行 `registerAction`。

### 决策 4：单线程事件循环

**决策**：整个事件管理中心采用单线程模型，不引入线程和锁。

**理由**：
- 10 下位机 × 20 Hz = 200 msg/s，单线程完全可处理
- 事件管理中心是共享状态的汇聚点，单线程天然避免锁竞争和数据竞争
- EventManager 处理路径纯内存操作，无阻塞 I/O

### 决策 5：EventSource 枚举区分事件来源

**决策**：新增 `EventSource::Device` 和 `EventSource::System` 枚举区分下位机事件和系统事件。

**理由**：Device 事件 ID 保持 `"P-F-A"` 格式不变；System 事件 ID 使用预定义的事件名（如 `"comm_lost"`）。Decouple 来源类型与 ID 格式，扩展新事件来源时不破坏现有格式。

### 决策 6：系统事件预定义列表

**决策**：系统事件名必须在 `backend/system_events.cpp` 中集中预定义，`triggerSystemEvent` 入口进行运行时校验。

**理由**：防止业务代码随意创建未定义的事件名，保障事件命名的规范性和可追溯性。

---

## 4. CSCI体系结构设计

### 4.1 CSCI部件

#### 4.1.1 事件值对象类型定义（CSC_EVENT_TYPES）

**文件**：`backend/event_types.h`

**用途**：定义本软件所有模块共用的枚举和结构体，是各模块间的数据类型契约。

**组成**：
| 类型 | 名称 | 说明 |
|------|------|------|
| 枚举 | `EventLevel` | Emergency=1, Serious=2, General=3, Info=4 |
| 枚举 | `EventState` | Active, Cleared |
| 枚举 | `EventSource` | Device, System |
| 结构体 | `AlarmDef` | 报警定义：id, description, originalLevel, 降级/屏蔽状态 |
| 结构体 | `SystemEventDef` | 系统事件定义：name, description, level |
| 结构体 | `Event` | 事件值对象：id, source, protocolID, frameID, alarmField, description, originalLevel, effectiveLevel, state, timestamp, 兜底联动列表 |

**依赖**：仅依赖 STL (`<string>`, `<vector>`)

#### 4.1.2 系统事件预定义列表（CSC_SYSTEM_EVENTS）

**文件**：`backend/system_events.h`, `backend/system_events.cpp`

**用途**：集中预定义所有系统事件，外部通过 `getSystemEventDefs()` 获取列表，通过 `findSystemEventDef(name)` 按名查找。

**关系**：被 `ExternalAPI::triggerSystemEvent` 调用进行运行时校验。

#### 4.1.3 对外接口门面（CSC_EXTERNAL_API）

**文件**：`backend/external_api.h`, `backend/external_api.cpp`

**用途**：本软件的唯一对外门面。提供工厂方法、事件产生/消除入口、查询接口。所有外部模块（NetworkReceiver、通信监测、observe 组件）仅通过此类交互。

**持有的引用**：`EventManager&`, `ConfigManager&`

**依赖**：`EventManager`, `ConfigManager`, `system_events`, `AlarmCatalog`

#### 4.1.4 事件管理中心（CSC_EVENT_MANAGER）

**文件**：`backend/event_manager.h`, `backend/event_manager.cpp`

**用途**：核心事件存储与调度。管理活跃事件的增删查、联动触发、前端通知、日志记录。

**持有引用**：`ConfigManager&`, `LinkageEngine&`

**核心存储**：`std::unordered_map<EventId, Event> activeEvents_`

**依赖**：`ConfigManager`, `LinkageEngine`, `SocketServer (桩)`, `LogWriter (桩)`

#### 4.1.5 配置管理（CSC_CONFIG_MANAGER）

**文件**：`backend/config_manager.h`, `backend/config_manager.cpp`

**用途**：管理降级映射表和屏蔽集合。提供设置/取消/查询接口。

**核心存储**：`std::unordered_map<EventId, EventLevel> downgradeMap_`, `std::unordered_set<EventId> shieldSet_`

**依赖**：仅依赖 `event_types.h`

#### 4.1.6 联动引擎（CSC_LINKAGE_ENGINE）

**文件**：`backend/linkage_engine.h`, `backend/linkage_engine.cpp`

**用途**：字符串名称注册制的联动动作执行引擎。维护动作表、事件联动配置表、等级默认表。`executeActive/executeCleared` 查表合并后按序执行回调。

**核心存储**：
- `std::unordered_map<string, ActionCallback> actionTable_`
- `std::unordered_map<EventId, pair<vector<string>,vector<string>>> eventConfig_`
- `std::unordered_map<int, vector<string>> levelDefaults_`

**依赖**：仅依赖 `event_types.h`

#### 4.1.7 能力注册（CSC_ACTION_REGISTRY）

**文件**：`backend/action_registry.h`, `backend/action_registry.cpp`

**用途**：启动时集中注册所有联动动作和配置事件联动关系。`static void setup(LinkageEngine&)` 一次性完成所有注册。

**依赖**：`LinkageEngine`, `CmdSender (桩)`, `BuzzerControl (桩)`, `SocketServer (桩)`

#### 4.1.8 一键启动模块（CSC_EVENTMGR_MODULE）

**文件**：`backend/event_mgr_module.h`, `backend/event_mgr_module.cpp`

**用途**：对外提供一键启动的静态方法。`init()` 创建所有核心实例并建立依赖关系，`api()` 和 `config()` 返回全局访问点。

**持有**：`ConfigManager*, LinkageEngine*, EventManager*, ExternalAPI*`（静态指针）

#### 4.1.9 前后端桥接（CSC_BACKEND_BRIDGE）

**文件**：`frontend/backend_bridge.h`, `frontend/backend_bridge.cpp`

**用途**：Qt 对象，持有所有后端实例，提供 Qt 风格的接口（QString/QVector），是前端与后端的唯一桥接点。分离模式下可替换为 Socket 代理。

**持有**：`ConfigManager*, LinkageEngine*, EventManager*, ExternalAPI*`

**依赖**：所有后端模块 + `ActionRegistry`

#### 4.1.10 前端主容器（CSC_EVENTMGR_WIDGET）

**文件**：`frontend/eventmgr_widget.h`, `frontend/eventmgr_widget.cpp`

**用途**：QWidget 子类，包含 QTabWidget 切换页（事件列表 + 报警配置）、底部屏蔽数量状态栏。监听 Tab 切换信号自动刷新目标页面。

**包含子控件**：`EventListWidget`, `AlarmCatalogWidget`

#### 4.1.11 活跃事件列表（CSC_EVENT_LIST）

**文件**：`frontend/event_list_widget.h`, `frontend/event_list_widget.cpp`, `frontend/event_list_widget.ui`

**用途**：7 列表格展示所有活跃事件（编号/描述/时间/等级/状态/降级/屏蔽）。降级/屏蔽列使用 QCheckBox 即时写入 ConfigManager。每 1 秒自动刷新，支持模拟报警调试。

**依赖**：`BackendBridge`

#### 4.1.12 报警配置页（CSC_ALARM_CATALOG）

**文件**：`frontend/alarm_catalog_widget.h`, `frontend/alarm_catalog_widget.cpp`, `frontend/alarm_catalog_widget.ui`

**用途**：5 列表格展示所有报警定义（编号/描述/等级/降级/屏蔽）。降级/屏蔽列使用 QCheckBox，"应用配置"按钮批量写入。

**依赖**：`BackendBridge`

#### 4.1.13 实时报警日志（CSC_ALARM_LOG）

**文件**：`frontend/alarm_log_widget.h`, `frontend/alarm_log_widget.cpp`, `frontend/alarm_log_widget.ui`

**用途**：2 列表格（时间/报警内容）展示当前活跃事件的实时日志。每 1 秒刷新，可嵌入主界面任意位置。

**依赖**：`BackendBridge`

---

### 4.2 执行方案

#### 4.2.1 告警产生流程（MS_ALARM_GENERATE）

```
observe / NetworkReceiver
    │ triggerAlarm(pid, fid, field, true)
    ▼
ExternalAPI::triggerAlarm
    │ 1. 构造 EventId "pid-fid-field"
    │ 2. 查 AlarmCatalog 获取 level + description
    │ 3. createAlarm() → Event
    │ 4. addEvent(event)
    ▼
EventManager::processAddEvent
    │ 1. 查重: activeEvents_.find(id) → 已存在则 return
    │ 2. effectiveLevel = configMgr_.getEffectiveLevel(id, originalLevel)
    │ 3. 生成 timestamp: strftime("%Y-%m-%d %H:%M:%S")
    │ 4. activeEvents_[id] = event
    │ 5. if (effectiveLevel != Info) linkageEng_.executeActive(event)
    │ 6. if (!isShielded) notifyFrontend(event, true)
    │ 7. writeLog(event, "发生")
    ▼
LinkageEngine::executeActive
    │ 1. event.effectiveLevel == Info → return
    │ 2. resolveActiveNames: 事件级配置 + 等级默认 → 合并
    │ 3. for each name → actionTable_[name]() 执行回调
    ▼
前端 QTimer 1s → refresh() → getActiveEvents() → 更新表格
```

#### 4.2.2 告警消除流程（MS_ALARM_CLEAR）

```
observe / NetworkReceiver
    │ triggerAlarm(pid, fid, field, false)
    ▼
ExternalAPI::triggerAlarm
    │ isActive == false → clearEvent(pid, fid, field)
    ▼
EventManager::processClearEvent
    │ 1. EventId = makeEventId(pid, fid, field)
    │ 2. activeEvents_.find(id) → 不存在则 return
    │ 3. event.state = Cleared
    │ 4. linkageEng_.executeCleared(event)
    │ 5. notifyFrontend(event, false)
    │ 6. writeLog(event, "消除")
    │ 7. activeEvents_.erase(id)
    ▼
前端 QTimer 1s → refresh() → 表格中该行消失
```

#### 4.2.3 系统事件流程（MS_SYSTEM_EVENT）

```
通信监测/系统监测
    │ triggerSystemEvent("comm_lost", 3, true)
    ▼
ExternalAPI::triggerSystemEvent
    │ 1. findSystemEventDef("comm_lost") → 验证通过
    │ 2. 构造 Event: source=System, id="comm_lost:3", protocolID=3
    │ 3. 设置 level/description 从预定义列表
    │ 4. addEvent(event) → 后续同 MS_ALARM_GENERATE
    ▼
EventManager::processAddEvent (同上)
```

#### 4.2.4 前端刷新流程（MS_FRONTEND_REFRESH）

```
Tab 切换事件
    │ QTabWidget::currentChanged(index)
    ▼
EventMgrWidget::onTabChanged
    │ index 0 → eventListPage_->refresh()
    │ index 1 → catalogPage_->loadCatalog()
    ▼
refresh() / loadCatalog()
    │ bridge_->getActiveEvents() / getCatalog()
    ▼
BackendBridge → ExternalAPI → EventManager / ConfigManager
    │ 返回当前数据
    ▼
更新 QTableWidget 行
```

---

### 4.3 接口设计

#### 4.3.1 接口标识和接口图

```
┌────────────────────────────────────────────┐
│                前端 Qt                      │
│  EventMgrWidget                             │
│   ├── EventListWidget  ──┐                  │
│   ├── AlarmCatalogWidget─┤                  │
│   └── AlarmLogWidget    ─┤                  │
│                          ▼                  │
│                   BackendBridge              │
│                   (JK_BRIDGE)                │
├────────────────────────────────────────────┤
│                后端 C++11                    │
│                   ExternalAPI                │
│                   (JK_API)                   │
│                      │                      │
│         ┌────────────┼────────────┐         │
│         ▼            ▼            ▼         │
│   EventManager  ConfigManager  LinkageEngine│
│         │                         ▲         │
│         │              ActionRegistry       │
│         ▼                                   │
│   ┌──────────┐                              │
│   │ 桩模块    │  SocketServer / LogWriter   │
│   │          │  CmdSender / BuzzerControl   │
│   └──────────┘                              │
└────────────────────────────────────────────┘

接口符号:
──► 调用方向
JK_API   ExternalAPI 对外接口
JK_BRIDGE BackendBridge 桥接接口
JK_SOCKET 前后端 Socket 通信协议
```

#### 4.3.2 ExternalAPI 接口（JK_API）

**接口标识**：`JK_API`

**通信方法**：直接 C++ 函数调用（同步）

**消息格式**：C++ 函数参数（值传递或 const 引用），返回值直接返回

| 方法 | 参数类型 | 返回类型 | 说明 |
|------|---------|---------|------|
| `createAlarm` | int, int, const string&, EventLevel, const string& | Event | 工厂方法 |
| `addEvent` | const Event& | void | 提交事件 |
| `triggerAlarm` | int, int, const string&, bool | void | observe 对接 |
| `triggerSystemEvent` | const string&, bool | void | 纯系统事件 |
| `triggerSystemEvent` | const string&, int, bool | void | 关联设备系统事件 |
| `clearEvent` | int, int, const string& | void | Device 消除 |
| `clearEvent` | const string& | void | 按 ID 消除 |
| `getActiveEvents` | — | vector\<Event\> | 查询活跃事件 |
| `getAlarmCatalog` | — | vector\<AlarmDef\> | 查询报警目录 |

**异常处理**：所有方法不抛异常；无效输入静默忽略

#### 4.3.3 BackendBridge 接口（JK_BRIDGE）

**接口标识**：`JK_BRIDGE`

**通信方法**：Qt 信号/槽 + 直接调用

**数据类型**：QString, QVector（Qt 风格）

| 方法 | 返回类型 | 说明 |
|------|---------|------|
| `initialize()` | void | 初始化后端 + ActionRegistry 注册 |
| `triggerAlarm(QString id, bool isActive)` | void | 解析 "P-F-A" 格式 id 后调 ExternalAPI |
| `getActiveEvents()` | QVector\<EventEntry\> | 获取活跃事件（含时间戳） |
| `getCatalog()` | QVector\<CatalogEntry\> | 获取报警目录及配置状态 |
| `setDowngrade/setShield/clearShield/...` | void | 配置操作 |
| `shieldCount()` | int | 屏蔽总数 |
| `api()` | ExternalAPI& | 获取后端 API 引用 |

**EventEntry 结构**：`{QString id, description, timestamp; int level; bool shielded}`

**CatalogEntry 结构**：`{QString id, description; int originalLevel, downgradeTo; bool downgraded, shielded}`

#### 4.3.4 前后端通信协议（JK_SOCKET）

**接口标识**：`JK_SOCKET`

**通信方法**：JSON 字符串通过 Socket 传输（当前一体模式下为直接函数调用，Socket 路径预留）

**告警产生消息**：
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

**告警消除消息**：
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

---

## 5. CSCI详细设计

### 5.1 ExternalAPI 详细设计（CSU_API）

**文件**：`backend/external_api.h`, `backend/external_api.cpp`

**设计决策**：作为唯一对外门面，所有外部调用均通过此类。内部持有 EventManager 和 ConfigManager 引用（非所有权）。

**设计约束**：
- 构造函数注入 EventManager& 和 ConfigManager&
- 不持有任何资源，析构函数为空
- 所有方法不抛异常

**执行过程**：

`triggerAlarm(pid, fid, field, isActive)`:
```
1. if (!isActive) → clearEvent(pid, fid, field); return
2. 构造 EventId = "pid-fid-field"
3. 遍历 AlarmCatalog::getAllDefinitions() 找到匹配的 AlarmDef
4. 若找到 → createAlarm(pid, fid, field, def.level, def.description) → addEvent
5. 若未找到 → createAlarm(pid, fid, field, Info, field) → addEvent（兜底）
```

`triggerSystemEvent(name, isActive)`:
```
1. def = findSystemEventDef(name)
2. if (!def) → return（打印警告）
3. if (!isActive) → clearEvent(name); return
4. 构造 Event {source=System, id=name, description=def.description, level=def.level}
5. addEvent(event)
```

`clearEvent(eventId)`:
```
1. 查找 eventId 中是否包含两个 '-'（Device 格式特征）
2. 含两个 '-' → 解析为 pid-fid-field → processClearEvent(pid, fid, field)
3. 否则 → processClearEvent(eventId)（System 事件精准匹配）
```

**类图参考**：`docs/superpowers/specs/2026-06-25-architecture-diagrams.md`

---

### 5.2 EventManager 详细设计（CSU_EVENT_MGR）

**文件**：`backend/event_manager.h`, `backend/event_manager.cpp`

**设计决策**：
- 核心存储使用 `unordered_map<EventId, Event>`，O(1) 查重
- 前端通知通过注入的 `NotifyCallback` 实现，未注入时走 SocketServer 桩（向后兼容）
- 日志写入通过 LogWriter 桩

**设计约束**：
- 单线程模型，无锁
- 构造函数接受 ConfigManager& 和 LinkageEngine& 引用

**核心存储**：
```
activeEvents_: unordered_map<EventId, Event>
```

**执行过程**：

`processAddEvent(event)`:
```
1. 查重: if (activeEvents_.find(event.id) != end) return
2. 生成时间戳: strftime(buf, 20, "%Y-%m-%d %H:%M:%S", localtime(&now))
3. 计算有效等级: configMgr_.getEffectiveLevel(event.id, event.originalLevel)
4. 存储: activeEvents_[id] = event
5. 联动: if (effectiveLevel != Info) linkageEng_.executeActive(event)
6. 通知: if (!configMgr_.isShielded(id)) notifyFrontend(event, true)
7. 日志: writeLog(event, "发生")
```

`processClearEvent(eventId)`:
```
1. 查找: it = activeEvents_.find(eventId); if (it == end) return
2. event.state = Cleared
3. linkageEng_.executeCleared(event)
4. notifyFrontend(event, false)
5. writeLog(event, "消除")
6. activeEvents_.erase(it)
```

---

### 5.3 LinkageEngine 详细设计（CSU_LINKAGE）

**文件**：`backend/linkage_engine.h`, `backend/linkage_engine.cpp`

**设计决策**：
- 字符串名称注册制：`registerAction(name, callback)` 注册 Lambda
- 三层配置：事件级 > 等级级 > 实例级（兜底）
- executeActive 在 effectiveLevel == Info 时跳过
- executeCleared 不自动镜像 active 动作（UI 锁控由前端处理）

**核心存储**：
```
actionTable_:    unordered_map<string, ActionCallback>
eventConfig_:    unordered_map<EventId, pair<vector<string>, vector<string>>>
levelDefaults_:  unordered_map<int, vector<string>>
```

**执行过程**：

`executeActive(event)`:
```
1. if (event.effectiveLevel == Info) return
2. names = resolveActiveNames(event)
3. for each name in names:
     if (actionTable_.find(name) != end) actionTable_[name]()
```

`resolveActiveNames(event)`:
```
1. 合并: eventConfig_[event.id].first + levelDefaults_[int(event.originalLevel)]
2. 若合并为空且 event 自带 activeActions → 返回 event.activeActions
3. 返回合并结果
```

---

### 5.4 ConfigManager 详细设计（CSU_CONFIG）

**文件**：`backend/config_manager.h`, `backend/config_manager.cpp`

**设计决策**：配置即时生效，不依赖事件是否已产生。降级和屏蔽为独立维度——降级影响 effectiveLevel（联动），屏蔽影响通知。

**核心存储**：
```
downgradeMap_: unordered_map<EventId, EventLevel>
shieldSet_:    unordered_set<EventId>
```

**执行过程**：

`getEffectiveLevel(id, originalLevel)`:
```
1. it = downgradeMap_.find(id)
2. if (it != end) return it->second
3. return originalLevel
```

---

### 5.5 ActionRegistry 详细设计（CSU_REGISTRY）

**文件**：`backend/action_registry.h`, `backend/action_registry.cpp`

**设计决策**：纯静态类，`static void setup(LinkageEngine&)` 集中完成所有注册。每个注册项是一个 Lambda 闭包，捕获各自所需的参数（控制器引用、目标设备、指令等）。

**执行过程**：

```
ActionRegistry::setup(engine):
1. registerAction("boiler_stop",  [](){ BoilerController::emergencyStop(); })
2. registerAction("cooler_stop",  [](){ CoolingTowerController::emergencyStop(); })
3. registerAction("cooler_fan",   [](){ CoolingTowerController::setFanSpeed(1200); })
4. configureEvent("1-3-temp_high", {"cooler_fan"}, {})
5. configureEvent("2-1-vibration", {"cooler_stop"}, {})
6. configureEvent("comm_lost",     {"cooler_stop"}, {})
7. setLevelDefault(Emergency, {"cooler_stop"})
```

---

### 5.6 前端控件详细设计（CSU_FRONTEND）

**文件**：`frontend/eventmgr_widget.*`, `frontend/event_list_widget.*`, `frontend/alarm_catalog_widget.*`, `frontend/alarm_log_widget.*`

**设计决策**：
- 前端控件均为 QWidget 子类，可嵌入任意父级窗口
- 通过 `BackendBridge` 与后端交互，不直接依赖后端类
- 降级/屏蔽复选框在两个 Tab 间通过 ConfigManager 共享状态
- Tab 切换时监听 `QTabWidget::currentChanged` 自动刷新

**类关系**：
```
EventMgrWidget
  ├── QTabWidget
  │     ├── EventListWidget    (Tab 0: 事件列表)
  │     └── AlarmCatalogWidget (Tab 1: 报警配置)
  ├── QLabel (屏蔽数量状态栏)
  └── QTimer (1s 状态栏刷新)

AlarmLogWidget  (独立控件，嵌入主界面)
```

**数据流**：
```
QTimer(1s) → refresh() → BackendBridge::getActiveEvents()
                       → clear + repopulate QTableWidget

Checkbox::clicked → BackendBridge::setDowngrade/setShield
                  → ConfigManager 写入
                  → refresh()
```

---

## 6. 需求可追踪性

### 6.1 需求到软件单元的追踪

| 需求编号 | 需求标题 | 实现单元 (CSU) | 关联部件 (CSC) |
|----------|---------|---------------|---------------|
| RX_ALARM_GENERATE | 告警产生 | CSU_API, CSU_EVENT_MGR | CSC_EXTERNAL_API, CSC_EVENT_MANAGER |
| RX_ALARM_CLEAR | 告警消除 | CSU_API, CSU_EVENT_MGR | CSC_EXTERNAL_API, CSC_EVENT_MANAGER |
| RX_EVENT_LINKAGE | 事件联动 | CSU_LINKAGE, CSU_REGISTRY | CSC_LINKAGE_ENGINE, CSC_ACTION_REGISTRY |
| RX_ALARM_DOWNGRADE | 告警降级 | CSU_CONFIG | CSC_CONFIG_MANAGER |
| RX_ALARM_SHIELD | 报警屏蔽 | CSU_CONFIG | CSC_CONFIG_MANAGER |
| RX_SYSTEM_EVENT | 系统事件 | CSU_API, CSU_EVENT_MGR | CSC_EXTERNAL_API, CSC_SYSTEM_EVENTS |
| RX_FRONTEND_UI | 前端UI | CSU_FRONTEND | CSC_EVENTMGR_WIDGET, CSC_EVENT_LIST, CSC_ALARM_CATALOG, CSC_ALARM_LOG |
| RX_interface | 外部接口 | CSU_API | CSC_EXTERNAL_API, CSC_BACKEND_BRIDGE |
| rx_in_interface | 内部接口 | CSU_EVENT_MGR, CSU_LINKAGE, CSU_CONFIG | 对应 CSC |
| rx_in_data | 内部数据 | CSU_EVENT_MGR, CSU_CONFIG, CSU_LINKAGE | CSC_EVENT_TYPES |
| rx_design | 设计约束 | 所有单元 | 所有部件 |
| rx_other | 其他需求 | 所有单元 | 所有部件 |

### 6.2 软件单元到需求的追踪

| 软件单元 | 满足的需求 |
|----------|-----------|
| CSU_API | RX_ALARM_GENERATE, RX_ALARM_CLEAR, RX_SYSTEM_EVENT, RX_interface |
| CSU_EVENT_MGR | RX_ALARM_GENERATE, RX_ALARM_CLEAR, RX_SYSTEM_EVENT, rx_in_data |
| CSU_LINKAGE | RX_EVENT_LINKAGE |
| CSU_REGISTRY | RX_EVENT_LINKAGE |
| CSU_CONFIG | RX_ALARM_DOWNGRADE, RX_ALARM_SHIELD, rx_in_data |
| CSU_FRONTEND | RX_FRONTEND_UI |

### 6.3 可追踪性矩阵

```
            RX_ALARM  RX_ALARM  RX_LINKAGE  RX_DOWN  RX_SHIELD  RX_SYS   RX_UI   RX_IF
            _GEN      _CLEAR                            _GRADE    _EVENT
CSU_API        ●        ●                                ●         ●        ●       ●
CSU_EVENT_MGR  ●        ●                                ●         ●
CSU_LINKAGE                        ●
CSU_REGISTRY                       ●
CSU_CONFIG                                    ●        ●
CSU_FRONTEND                                                             ●
```
