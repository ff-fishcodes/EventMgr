# 事件管理中心架构与行为图

<a id="legend-boundary"></a>
## 1. 图例与边界

| 项目 | 内容 |
|---|---|
| 文档 ID | `EventMgr-AD-20260721` |
| 版本 | `1.2` |
| 日期 | 2026-07-23 |
| 状态 | 当前实现审计基线（架构与行为图） |
| 审计源码基线 | `89dad8b` |
| 当前需求基线 | [事件管理中心当前需求基线](./2026-07-21-software-requirements-baseline.md) |
| 概要设计 | [事件管理中心概要设计](./2026-07-21-high-level-design.md) |
| 详细设计 | [事件管理中心详细设计](./2026-07-21-detailed-design.md) |
| 证据范围 | 根目录 `main.cpp`，`backend/`、`backend/stubs/`、`frontend/` 中的当前 `.h/.cpp` |

本文只表达审计源码基线中已经存在的类、方法与调用。标有“桩”的节点只代表当前仓库中的占位实现。时序图中的“源码后续顺序”表示代码文本规定的逻辑顺序，不等于默认 GUI 同线程路径能够运行到该位置。

不同图型的箭头含义分别如下，不能跨图型套用：

- **UML 类图**：实线关联表示成员引用、实际持有或 Qt parent-child；`*--` 只用于源码中确实存储的值或拥有对象；虚线依赖表示创建、注册、直接/静态方法调用或返回类型依赖。
- **流程图**：实线表示当前进程内直接调用或委托；虚线表示信号、callback、注册或条件性创建/调用。边标签说明条件，例如 `fallback_ 已设置`。
- **时序图**：实线消息表示同步调用，虚线返回表示返回或异步入池；`opt`/`alt` 明确可选或互斥路径。

本轮图表重建的讨论、评审和验证过程已收入完成并审查的[文档讨论与验证记录](./2026-07-21-documentation-discussion-record.md)，图表与全量交付检查见其[最终验证结果](./2026-07-21-documentation-discussion-record.md#112-task-8-验证结果)。并发、生命周期和错误边界的文字依据见[详细设计第 9 节](./2026-07-21-detailed-design.md#core-algorithms)、[第 10 节](./2026-07-21-detailed-design.md#ownership-lifetime)、[第 11 节](./2026-07-21-detailed-design.md#concurrency-matrix)和[第 12 节](./2026-07-21-detailed-design.md#error-boundaries)。

**目录**

- [1. 图例与边界](#legend-boundary)
- [2. 类图](#class-views)：[总体关系](#class-overview)、[后端与领域](#class-backend-domain)、[前端与桥接](#class-frontend-bridge)
- [3. 调用关系图](#call-views)：[事件生命周期](#call-event-lifecycle)、[查询与配置](#call-query-config)
- [4. 模块初始化时序图](#sequence-init)
- [5. 设备告警产生时序图](#sequence-active)
- [6. 告警消除时序图](#sequence-clear)
- [7. 降级与屏蔽配置时序图](#sequence-config)
- [8. 联动动作异步执行时序图](#sequence-async)

<a id="class-views"></a>
## 2. 类图

<a id="class-overview"></a>
### 2.1 总体关系

```mermaid
classDiagram
    direction LR
    class EventMgrModule
    class ExternalAPI
    class EventManager
    class ConfigManager
    class LinkageEngine
    class ActionRegistry
    class BackendBridge
    class EventMgrWidget
    class EventListWidget
    class AlarmCatalogWidget
    class AlarmLogWidget

    EventMgrModule ..> ConfigManager : creates / setInstance
    EventMgrModule ..> LinkageEngine : creates / setInstance
    EventMgrModule ..> EventManager : creates / setInstance
    EventMgrModule ..> ExternalAPI : creates / setInstance
    EventMgrModule ..> ActionRegistry : setup
    ExternalAPI --> EventManager : reference / delegate
    ExternalAPI --> ConfigManager : reference
    EventManager --> ConfigManager : reference
    EventManager --> LinkageEngine : reference
    BackendBridge ..> EventMgrModule : initialize
    BackendBridge ..> ExternalAPI : instance calls
    BackendBridge ..> EventManager : callback injection
    BackendBridge ..> ConfigManager : instance calls
    BackendBridge ..> LinkageEngine : instance calls
    EventMgrWidget *-- BackendBridge : QObject parent
    EventMgrWidget *-- EventListWidget : QObject parent
    EventMgrWidget *-- AlarmCatalogWidget : QObject parent
    EventListWidget --> BackendBridge : non-owning
    AlarmCatalogWidget --> BackendBridge : non-owning
    AlarmLogWidget --> BackendBridge : non-owning / host-created only
```

此图只显示模块边界。`AlarmLogWidget` 是可独立嵌入控件，当前 `EventMgrWidget::setupUI()` 不创建它。

<a id="class-backend-domain"></a>
### 2.2 后端与领域类型

```mermaid
classDiagram
    direction TB
    class EventMgrModule {
        -ConfigManager* configMgr_$
        -LinkageEngine* linkageEng_$
        -EventManager* eventMgr_$
        -ExternalAPI* api_$
        +init() void$
        +api() ExternalAPI&$
        +config() ConfigManager&$
    }
    class ExternalAPI {
        -EventManager& eventMgr_
        -ConfigManager& configMgr_
        +instance() ExternalAPI&$
        +triggerAlarm(..., bool) void
        +triggerSystemEvent(...) void
        +addEvent(const Event&) void
        +clearEvent(...) void
        +getActiveEvents() vector~Event~
        +getAlarmCatalog() vector~AlarmDef~
    }
    class EventManager {
        +NotifyCallback
        -unordered_map~EventId, Event~ activeEvents_
        -QMutex mutex_
        +instance() EventManager&$
        +setNotifyCallback(NotifyCallback) void
        +processAddEvent(const Event&) void
        +processClearEvent(...) void
        +const Event* findEvent(const EventId&) const
        +getActiveEvents() vector~Event~
    }
    class ConfigManager {
        -unordered_map~EventId, EventLevel~ downgradeMap_
        -unordered_set~EventId~ shieldSet_
        -QMutex mutex_
        +instance() ConfigManager&$
        +setDowngrade(...) void
        +getEffectiveLevel(...) EventLevel
        +setShield(EventId) void
        +isShielded(EventId) bool
    }
    class LinkageEngine {
        +ActionCallback
        +FallbackCallback
        -QMutex configMutex_
        -QThreadPool linkagePool_
        +instance() LinkageEngine&$
        +setFallback(FallbackCallback) void
        +executeActive(const Event&) void
        +executeCleared(const Event&) void
        +getEventActionGroups(...) EventActionGroups
        +clearAll() void
    }
    class QMutex
    class ActionRegistry {
        +setup(LinkageEngine&) void$
    }
    class ActionTask {
        -shared_ptr~const ActionCallback~ callback_
        +run() void
    }
    class LevelActionConfig {
        +vector~string~ activeActions
        +vector~string~ clearActions
    }
    class LinkageEventActionGroups["LinkageEngine::EventActionGroups"] {
        +vector~ActionInfo~ activeActions
        +vector~ActionInfo~ clearActions
    }
    class Event {
        +EventId id
        +EventSource source
        +EventLevel originalLevel
        +EventLevel effectiveLevel
        +EventState state
        +vector~string~ activeActions
        +vector~string~ clearActions
    }
    class AlarmDef {
        +EventId id
        +EventLevel originalLevel
        +bool downgraded
        +EventLevel downgradeTo
        +bool shielded
    }
    class SystemEventDef {
        +string name
        +string description
        +EventLevel level
    }
    class ActionInfo {
        +string name
        +string displayName
        +bool enabled
    }
    class AlarmCatalog {
        <<stub>>
    }
    class SocketServer {
        <<stub>>
    }
    class LogWriter {
        <<stub>>
    }
    class CmdSender {
        <<stub>>
    }
    class BuzzerControl {
        <<stub>>
    }

    EventMgrModule ..> ExternalAPI : creates / setInstance
    EventMgrModule ..> EventManager : creates / setInstance
    EventMgrModule ..> ConfigManager : creates / setInstance
    EventMgrModule ..> LinkageEngine : creates / setInstance
    EventMgrModule ..> ActionRegistry : setup
    ExternalAPI --> EventManager : reference
    ExternalAPI --> ConfigManager : reference
    ExternalAPI ..> AlarmCatalog : static catalog lookup
    ExternalAPI ..> SystemEventDef : system event lookup
    ExternalAPI ..> AlarmDef : returns by value
    EventManager --> ConfigManager : reference
    EventManager --> LinkageEngine : reference
    EventManager *-- Event : activeEvents_ stored values
    EventManager ..> SocketServer : static call if notifyCb_ empty
    EventManager ..> LogWriter : static log call
    LinkageEngine ..> ActionInfo : nested return type
    LinkageEngine ..> LinkageEventActionGroups : returns by value
    LinkageEngine *-- LevelActionConfig : levelDefaults_ values
    LinkageEngine *-- QMutex : configMutex_
    LinkageEngine ..> ActionTask : creates per action
    ActionRegistry ..> LinkageEngine : registers callbacks/config
    ActionRegistry ..> CmdSender : registered lambdas call
    ActionRegistry ..> BuzzerControl : registered lambda calls
```

`EventManager *-- Event` 表示哈希表确实存储 `Event` 值；其他返回值类型只用虚线依赖。四个后端对象由静态模块启动且没有 shutdown。

<a id="class-frontend-bridge"></a>
### 2.3 前端与桥接类型

```mermaid
classDiagram
    direction TB
    class BackendBridge {
        +initialize() void
        +triggerAlarm(QString, bool) void
        +getActiveEvents() QVector~EventEntry~
        +getCatalog() QVector~CatalogEntry~
        +setDowngrade(...) void
        +setShield(QString) void
        +getEventActionGroups(...) EventActionGroups
        +eventsChanged() signal
        +linkageAction(QString, bool) signal
    }
    class EventEntry {
        +QString id
        +QString description
        +QString timestamp
        +int level
        +bool downgraded
        +bool shielded
    }
    class CatalogEntry {
        +QString id
        +QString description
        +int originalLevel
        +bool downgraded
        +int downgradeTo
        +bool shielded
    }
    class ActionEntry {
        +QString name
        +QString displayName
        +bool enabled
    }
    class BridgeEventActionGroups["BackendBridge::EventActionGroups"] {
        +QVector~ActionEntry~ activeActions
        +QVector~ActionEntry~ clearActions
    }
    class EventMgrWidget {
        -BackendBridge* bridge_
        -EventListWidget* eventListPage_
        -AlarmCatalogWidget* catalogPage_
        -QTimer* statusTimer_
        +updateShieldStatus() slot
    }
    class EventListWidget {
        -BackendBridge* bridge_
        -QTimer* refreshTimer_
        +refresh() slot
    }
    class AlarmCatalogWidget {
        -BackendBridge* bridge_
        -QMap~QString,PendingEventConfig~ pendingByEvent_
        -QMap~QString,EventActionGroups~ actionGroupsByEvent_
        +loadCatalog() slot
        +requestReload() slot
        +requestLeave() bool
        +on_applyBtn_clicked() slot
    }
    class PendingEventConfig
    class ElidedLabel {
        -QString fullText_
        -QString displayText_
        +paintEvent(...)
        +resizeEvent(...)
        +changeEvent(...)
    }
    class AlarmLogWidget {
        -BackendBridge* bridge_
        -QTimer* refreshTimer_
        +refresh() slot
    }
    class ExternalAPI
    class EventManager
    class ConfigManager
    class LinkageEngine
    class LinkageEventActionGroups["LinkageEngine::EventActionGroups"]

    BackendBridge ..> EventEntry : nested return-by-value DTO
    BackendBridge ..> CatalogEntry : nested return-by-value DTO
    BackendBridge ..> ActionEntry : nested return-by-value DTO
    BackendBridge ..> BridgeEventActionGroups : nested return-by-value DTO
    BackendBridge ..> ExternalAPI : singleton calls
    BackendBridge ..> EventManager : callback injection
    BackendBridge ..> ConfigManager : singleton calls
    BackendBridge ..> LinkageEngine : singleton calls
    BackendBridge ..> LinkageEventActionGroups : converts grouped result
    EventMgrWidget *-- BackendBridge : QObject parent
    EventMgrWidget *-- EventListWidget : QObject parent
    EventMgrWidget *-- AlarmCatalogWidget : QObject parent
    EventListWidget --> BackendBridge : non-owning
    AlarmCatalogWidget --> BackendBridge : non-owning
    AlarmCatalogWidget --> PendingEventConfig : pendingByEvent_ values
    AlarmCatalogWidget --> BridgeEventActionGroups : actionGroupsByEvent_ values
    AlarmCatalogWidget *-- ElidedLabel : action cell QObject parent
    AlarmLogWidget --> BackendBridge : non-owning / host-created only
```

三个 DTO 是 `BackendBridge` 的嵌套类型并按值临时返回，虚线只表示类型依赖，不表示桥接长期存储或拥有 DTO。默认容器只拥有桥接、事件列表和目录页；日志控件需宿主另行创建。

<a id="call-views"></a>
## 3. 调用关系图

<a id="call-event-lifecycle"></a>
### 3.1 事件生命周期与通知

```mermaid
flowchart TB
    subgraph Input["触发入口"]
        OBS["observe / 宿主"]
        SIM["EventListWidget::on_simBtn_clicked"]
        SYS["系统监测调用方"]
    end
    subgraph Lifecycle["进程内生命周期"]
        BBTA["BackendBridge::triggerAlarm"]
        TA["ExternalAPI::triggerAlarm"]
        TS["ExternalAPI::triggerSystemEvent"]
        FIND["findSystemEventDef(eventName)"]
        AE["ExternalAPI::addEvent"]
        CE["ExternalAPI::clearEvent"]
        PA["EventManager::processAddEvent"]
        PC["EventManager::processClearEvent"]
        CFG["ConfigManager"]
        LE["LinkageEngine"]
        POOL["QThreadPool / ActionTask"]
    end
    subgraph Notify["通知与当前桩"]
        NF["EventManager::notifyFrontend"]
        NCB["BackendBridge 注入 NotifyCallback"]
        SIGNAL["BackendBridge::eventsChanged"]
        SOCK["SocketServer::notifyFrontend 桩"]
        FCB["BackendBridge 注入 FallbackCallback"]
        LKSIG["BackendBridge::linkageAction"]
        LOG["LogWriter::write 桩"]
        SYSWARN["LogWriter::write\n未注册系统事件警告"]
        IGNORE["return / 忽略"]
        CMD["CmdSender / BuzzerControl 桩"]
    end
    subgraph Consumers["eventsChanged 消费者"]
        LIST["EventListWidget::refresh\n默认连接顺序 1"]
        STATUS["EventMgrWidget::updateShieldStatus\n默认连接顺序 2"]
        ALOG["AlarmLogWidget::refresh\n仅宿主另行构造后连接"]
    end

    OBS --> TA
    SIM --> BBTA --> TA
    SYS --> TS --> FIND
    TA -->|"产生"| AE
    TA -->|"消除"| CE
    FIND -->|"未注册"| SYSWARN --> IGNORE
    FIND -->|"已注册且产生"| AE
    FIND -->|"已注册且消除"| CE
    AE --> PA
    CE --> PC
    PA --> CFG
    PA --> LE
    PC --> LE
    LE --> POOL --> CMD
    LE -.->|"仅 fallback_ 已设置"| FCB
    FCB -.-> LKSIG
    PA -->|"未屏蔽"| NF
    PC --> NF
    NF -.->|"notifyCb_ 已设置"| NCB
    NF -->|"notifyCb_ 为空"| SOCK
    NCB -.-> SIGNAL
    SIGNAL -.->|"默认第 1 个 direct slot"| LIST
    SIGNAL -.->|"默认第 2 个 direct slot"| STATUS
    SIGNAL -.->|"条件性连接；顺序取决于宿主构造时机"| ALOG
    PA --> LOG
    PC --> LOG
```

系统事件在产生/消除分流前先执行 `findSystemEventDef()`；未注册名称只写警告并返回，不进入事件生命周期。GUI 初始化会设置 fallback；根目录后端演示只调用 `EventMgrModule::init()`，不创建 `BackendBridge`，因此 `fallback_` 保持空。默认 `EventMgrWidget` 仅连接列表刷新后再连接状态刷新；同线程发射时，第一个槽重入事件锁并死锁，当前这次发射不会继续到状态槽。`AlarmLogWidget` 不在默认容器中；宿主若另行创建，它在自身构造函数中连接，连接顺序取决于构造时机。

<a id="call-query-config"></a>
### 3.2 查询与配置

```mermaid
flowchart TB
    subgraph UI["页面与宿主"]
        LIST["EventListWidget"]
        CATALOG["AlarmCatalogWidget"]
        ALOG["AlarmLogWidget\n宿主可选创建"]
    end
    subgraph Bridge["BackendBridge"]
        BBA["getActiveEvents"]
        BBC["getCatalog"]
        BBG["getEventActionGroups(id, level)"]
        BBCFG["set/removeDowngrade\nset/clearShield\nenable/disableAction"]
    end
    subgraph Backend["后端查询与配置"]
        APIA["ExternalAPI::getActiveEvents"]
        APIC["ExternalAPI::getAlarmCatalog"]
        EQ["EventManager::getActiveEvents"]
        CFG["ConfigManager"]
        CAT["AlarmCatalog::getAllDefinitions 桩"]
        LEG["LinkageEngine::getEventActionGroups"]
        MUTEX["configMutex_：单次分组快照"]
    end
    subgraph Pending["配置页暂存与提交"]
        LOAD["reloadFromBackend\n建立 original/current"]
        SELECT["switchSelectedEvent\n只重绘两阶段详情"]
        APPLY["on_applyBtn_clicked\n遍历脏事件"]
        LIVE["live group query\n过滤删除/换侧"]
        DIFF["applyActionDiffs\n逐变化项写入"]
    end

    LIST --> BBA
    ALOG --> BBA
    BBA --> APIA --> EQ
    LIST -->|"即时复选框"| BBCFG
    CATALOG --> LOAD
    LOAD --> BBC --> APIC
    APIC --> CAT
    APIC --> CFG
    LOAD --> BBG --> LEG --> MUTEX
    LOAD --> SELECT
    CATALOG --> APPLY --> LIVE --> BBG
    LIVE --> DIFF --> BBCFG --> CFG
    BBCFG --> LEG
```

活跃查询返回 `Event` 副本，再由桥接映射为 `EventEntry`。配置页为每个目录事件实际调用分组查询并缓存两阶段 DTO；应用前再次查询 live membership，再通过多个独立写接口提交差异。该流程 best-effort 保留未触碰变化，但不是事务且没有回滚。

<a id="sequence-init"></a>
## 4. 模块初始化时序图

```mermaid
sequenceDiagram
    participant Host as "frontend/main.cpp / 宿主"
    participant Widget as EventMgrWidget
    participant Bridge as BackendBridge
    participant Module as EventMgrModule
    participant CFG as ConfigManager
    participant LE as LinkageEngine
    participant EM as EventManager
    participant API as ExternalAPI
    participant Registry as ActionRegistry
    participant List as EventListWidget
    participant Catalog as AlarmCatalogWidget
    participant StatusTimer as "QTimer(statusTimer_)"

    Host->>Widget: new EventMgrWidget(parent)
    Widget->>Bridge: new BackendBridge(this)
    Widget->>Bridge: initialize()
    Bridge->>Module: init()
    alt api_ == NULL（首次初始化）
        Module->>CFG: new ConfigManager()
        Module->>LE: new LinkageEngine()
        Note right of LE: linkagePool_.setMaxThreadCount(4)
        Module->>EM: new EventManager(CFG, LE)
        Module->>API: new ExternalAPI(EM, CFG)
        Module->>CFG: ConfigManager::setInstance(configMgr_)
        Module->>LE: LinkageEngine::setInstance(linkageEng_)
        Module->>EM: EventManager::setInstance(eventMgr_)
        Module->>API: ExternalAPI::setInstance(api_)
        Module->>Registry: setup(LE)
        Registry->>LE: registerAction(...) x5
        Registry->>LE: configureEvent(...) x3
        Registry->>LE: setLevelDefault(Emergency, active, clear)
    else api_ != NULL（重复初始化）
        Module-->>Bridge: 立即 return，不再分配或注册
    end
    Bridge->>EM: instance().setNotifyCallback(lambda capturing this)
    Bridge->>LE: instance().setFallback(lambda capturing this)
    Note over Bridge,LE: 重复 initialize 仍覆盖两个回调；最后一次调用的 bridge 生效
    Bridge-->>Widget: initialize() 返回
    Widget->>Widget: setupUI()
    Widget->>List: new EventListWidget(bridge_, this)
    Widget->>Catalog: new AlarmCatalogWidget(bridge_, this)
    Widget->>Bridge: connect eventsChanged -> List::refresh（顺序 1）
    Widget->>Bridge: connect eventsChanged -> Widget::updateShieldStatus（顺序 2）
    Widget->>StatusTimer: new / timeout -> updateShieldStatus / start(1000)
    Note over Widget,Catalog: 当前 setupUI 不创建 AlarmLogWidget
```

**解释与风险**：`api_` 检查无同步，不能作为并发安全的一次性初始化。重复调用只跳过四个对象的分配和 `ActionRegistry::setup()`，不会跳过桥接回调覆盖。默认 `eventsChanged` 连接在页面构造之后由 `EventMgrWidget` 依次建立，而不是由 `EventListWidget` 构造函数建立。lambda 捕获裸 `this`，析构不解绑；模块也没有 shutdown、delete 或单例槽复位流程，因此存在悬空回调、泄漏式进程生命周期和“最后 bridge 赢”行为。

<a id="sequence-active"></a>
## 5. 设备告警产生时序图

```mermaid
sequenceDiagram
    participant Host as "observe / 宿主"
    participant Sim as "EventListWidget::on_simBtn_clicked"
    participant Bridge as BackendBridge
    participant API as ExternalAPI
    participant Catalog as AlarmCatalog
    participant EM as EventManager
    participant CFG as ConfigManager
    participant LE as LinkageEngine
    participant Pool as QThreadPool
    participant Fallback as "FallbackCallback\nGUI: BackendBridge lambda"
    participant UI as "EventListWidget::refresh"
    participant Status as "EventMgrWidget::updateShieldStatus"
    participant Socket as SocketServer
    participant Log as LogWriter

    alt observe / 宿主直接入口
        Host->>API: triggerAlarm(device, frame, field, true)
    else UI 模拟入口
        Sim->>Bridge: triggerAlarm(id, true)
        Bridge->>Bridge: split('-')，必须恰好 3 段
        Bridge->>API: instance().triggerAlarm(device, frame, field, true)
    end
    API->>Catalog: getAllDefinitions()
    alt 目录命中
        API->>API: createAlarm(...originalLevel, description)
    else 目录未命中
        API->>Log: write(提示等级警告)
        API->>API: createAlarm(...Info, alarmField)
    end
    API->>EM: processAddEvent(event)
    EM->>EM: QMutexLocker(non-recursive)
    alt activeEvents_ 已有相同 id
        EM-->>API: 静默返回；无联动、通知、日志
    else 新事件
        EM->>CFG: getEffectiveLevel(id, originalLevel)
        CFG-->>EM: 当前 effectiveLevel
        EM->>EM: state=Active / timestamp / 存入 activeEvents_
        EM->>LE: executeActive(stored)
        alt originalLevel == Info
            LE-->>EM: 直接返回；无动作且无 fallback
        else 非 Info
            LE->>LE: 锁内先追加 originalLevel 产生默认，再追加配置 active 或 event.activeActions，稳定去重
            LE->>LE: 解锁后再次锁定，按产生侧禁用状态快照 callback 句柄，再解锁
            LE-->>Pool: 锁外 start(ActionTask) 0..n
            opt fallback_ 已设置
                LE->>Fallback: fallback_(id, true)，调用线程同步
                opt 当前 GUI 注入目标
                    Fallback->>Bridge: emit linkageAction(id, true)
                end
            end
            LE-->>EM: 返回，不等待 ActionTask
        end
        EM->>CFG: isShielded(id)
        alt 已屏蔽
            Note over EM: 跳过 notifyFrontend
            EM->>Log: write(device-description-发生)
            EM-->>API: 解锁并返回
        else 未屏蔽
            EM->>EM: notifyFrontend(stored, true)，构造 JSON
            alt notifyCb_ 已设置，默认 GUI 同线程
                EM->>Bridge: 调用注入的 NotifyCallback lambda(JSON)
                Bridge->>UI: emit eventsChanged()，AutoConnection 直接调用
                UI->>Bridge: getActiveEvents()
                Bridge->>API: instance().getActiveEvents()
                API->>EM: getActiveEvents()
                EM->>EM: 再次锁同一非递归 QMutex
                Note over EM,UI: 默认运行在此死锁；不会到达“发生”日志和解锁
                Note over Bridge,Status: 当前发射的第 2 个 direct slot 不会被调用
                Note over Bridge,Status: AlarmLogWidget 默认未创建；宿主另建时连接顺序取决于构造时机
            else notifyCb_ 已设置但不重入 / 跨线程排队
                EM->>Bridge: 调用注入的 NotifyCallback lambda(JSON)
                Bridge-->>EM: 回调返回
                EM->>Log: write(device-description-发生)
                EM-->>API: 解锁并返回
            else notifyCb_ 为空
                EM->>Socket: SocketServer::notifyFrontend(JSON)
                Socket-->>EM: 桩调用返回
                EM->>Log: write(device-description-发生)
                EM-->>API: 解锁并返回
            end
        end
    end
```

**解释与风险**：图严格区分源码逻辑顺序与默认运行停点。有效等级只在加入时计算并保存；联动等级默认仍按 `originalLevel`。fallback 只有 `fallback_` 已设置时才同步调用；GUI 桥接会注入发射 `linkageAction` 的 lambda，根目录后端演示不注入。产生侧屏蔽只抑制通知，不抑制入表、联动和源码后续日志。默认容器先连接 `EventListWidget::refresh`，再连接 `EventMgrWidget::updateShieldStatus`；第一个 direct slot 重入死锁后，本次发射到不了第二个槽。`AlarmLogWidget` 默认不存在，宿主另行创建时才成为消费者。若通知回调不重入、跨线程连接实际排队，或没有通知回调而走 `SocketServer` 桩，源码后续顺序才可完成。

<a id="sequence-clear"></a>
## 6. 告警消除时序图

```mermaid
sequenceDiagram
    participant Caller as "observe / 系统调用方"
    participant API as ExternalAPI
    participant EM as EventManager
    participant LE as LinkageEngine
    participant Pool as QThreadPool
    participant Fallback as "FallbackCallback\nGUI: BackendBridge lambda"
    participant Bridge as BackendBridge
    participant UI as "EventListWidget::refresh"
    participant Status as "EventMgrWidget::updateShieldStatus"
    participant Socket as SocketServer
    participant Log as LogWriter

    Caller->>API: clearEvent(...) 或 triggerAlarm(..., false)
    API->>EM: processClearEvent(三字段或 EventId)
    EM->>EM: QMutexLocker(non-recursive) / find
    alt 未找到事件
        EM-->>API: 静默返回，无其他副作用
    else 找到事件
        EM->>EM: event.state = Cleared
        EM->>LE: executeCleared(event)
        alt originalLevel == Info
            LE-->>EM: 无动作且无 fallback
        else 非 Info
            LE->>LE: 锁内先追加 originalLevel 消除默认，再追加配置 clear 或 event.clearActions，稳定去重
            LE->>LE: 解锁后再次锁定，按消除侧禁用状态快照 callback 句柄，再解锁
            LE-->>Pool: 锁外 start(ActionTask) 0..n
            opt fallback_ 已设置
                LE->>Fallback: fallback_(id, false)，调用线程同步
                opt 当前 GUI 注入目标
                    Fallback->>Bridge: emit linkageAction(id, false)
                end
            end
            LE-->>EM: 返回，不等待动作
        end
        Note over EM: 清除通知不检查 shielded
        EM->>EM: notifyFrontend(event, false)，构造 alarm_cleared JSON
        alt notifyCb_ 已设置，默认 GUI 同线程
            EM->>Bridge: 调用注入的 NotifyCallback lambda(JSON)
            Bridge->>UI: emit eventsChanged()，直接调用 refresh()
            UI->>Bridge: getActiveEvents()
            Bridge->>API: instance().getActiveEvents()
            API->>EM: getActiveEvents()
            EM->>EM: 再锁同一非递归 QMutex
            Note over EM,UI: 死锁；条目保持 Cleared 但仍在 activeEvents_
            Note over EM,Log: “消除”日志与 erase 均未到达
            Note over Bridge,Status: 当前发射的第 2 个 direct slot 不会被调用
            Note over Bridge,Status: AlarmLogWidget 默认未创建；宿主另建时连接顺序取决于构造时机
        else notifyCb_ 已设置但不重入 / 跨线程排队
            EM->>Bridge: 调用注入的 NotifyCallback lambda(JSON)
            Bridge-->>EM: 回调返回
            EM->>Log: write(device-description-消除)
            EM->>EM: activeEvents_.erase(it)
            EM-->>API: 解锁并返回
        else notifyCb_ 为空
            EM->>Socket: SocketServer::notifyFrontend(JSON)
            Socket-->>EM: 桩调用返回
            EM->>Log: write(device-description-消除)
            EM->>EM: activeEvents_.erase(it)
            EM-->>API: 解锁并返回
        end
    end
```

**解释与风险**：清除联动的 fallback 同样只在 `fallback_` 已设置时同步调用。清除命中后，无论事件是否屏蔽都同步通知。默认同线程 UI 在第一个连接的列表刷新中重入查询并停止；状态槽不会在本次发射中执行，事件已被改成 `Cleared`，但尚未写日志和删除。宿主另行创建的 `AlarmLogWidget` 也会连接 `eventsChanged`，其相对顺序由构造时机决定。只有无重入的通知路径才会完成“联动调度 -> 通知 -> 日志 -> erase”的源码顺序。两个 `processClearEvent` 重载在锁内行为相同。

<a id="sequence-config"></a>
## 7. 配置暂存、统一应用与离开保护时序图

```mermaid
sequenceDiagram
    participant User as 用户
    participant Container as EventMgrWidget
    participant Active as EventListWidget
    participant Catalog as AlarmCatalogWidget
    participant AlarmLog as AlarmLogWidget
    participant Bridge as BackendBridge
    participant CFG as ConfigManager
    participant API as ExternalAPI
    participant EM as EventManager
    participant EventListTimer as "QTimer(EventListWidget::refreshTimer_)"
    participant StatusTimer as "QTimer(EventMgrWidget::statusTimer_)"
    participant AlarmLogTimer as "QTimer(AlarmLogWidget::refreshTimer_)"

    alt 活跃列表复选框：点击即生效
        alt 降级复选框
            User->>Active: 选中或取消降级
            Active->>Bridge: setDowngrade(id, 4) 或 removeDowngrade(id)
            Bridge->>CFG: 对应降级方法
        else 正常屏蔽路径
            Note over User,Active: 可见行必为未屏蔽，复选框初始未选中
            User->>Active: 选中屏蔽
            Active->>Bridge: setShield(id)
            Bridge->>CFG: setShield(id)
        else 已实现的取消屏蔽 callback/API 分支
            Note over User,Active: lambda 支持 checked=false，但屏蔽行在重建后被跳过，通常无法点击
            Active->>Bridge: clearShield(id)
            Bridge->>CFG: clearShield(id)
        end
        CFG->>CFG: QMutexLocker / 修改单个内存容器 / 解锁
        Bridge-->>Active: 返回
        Active->>Active: refresh()
        Active->>Bridge: getActiveEvents()
        Bridge->>API: instance().getActiveEvents()
        API->>EM: getActiveEvents()（EventManager 锁）
        EM-->>API: std::vector<Event> 副本
        API-->>Bridge: std::vector<Event>
        loop 每个事件
            Bridge->>CFG: hasDowngrade(id)（单次加锁）
            Bridge->>CFG: isShielded(id)（单次加锁）
            Bridge->>Bridge: 映射字段并 append EventEntry
        end
        Bridge-->>Active: QVector<EventEntry>
        Active->>Active: shielded 条目不插入表格
        Note over User,Catalog: 正常取消屏蔽通过报警目录页完成
    else 报警目录：编辑后批量应用
        User->>Catalog: 修改多行复选框
        Note over Catalog: 此时只改控件状态，尚未写配置
        User->>Catalog: on_applyBtn_clicked()
        loop 表格每一行
            Catalog->>Bridge: set/removeDowngrade(id, 4)
            Bridge->>CFG: 对应方法（各自独立加锁）
            Catalog->>Bridge: set/clearShield(id)（正常取消屏蔽入口）
            Bridge->>CFG: 对应方法（各自独立加锁）
        end
        Catalog->>Catalog: loadCatalog()
        Catalog->>Bridge: getCatalog()
        Bridge->>API: instance().getAlarmCatalog()
        API->>API: AlarmCatalog 定义 + SystemEventDef
        loop 每个目录定义
            API->>CFG: hasDowngrade / getEffectiveLevel / isShielded
        end
        API-->>Bridge: std::vector<AlarmDef>
        loop 每个 AlarmDef
            Bridge->>Bridge: 映射字段并 append CatalogEntry
        end
        Bridge-->>Catalog: QVector<CatalogEntry>
    end

    Note over CFG,EM: 活跃后更改降级只改 downgradeMap_
    Note over CFG,EM: 已存 Event.effectiveLevel、现有等级文字/颜色不重写；downgraded 标志会变化
    Note over CFG,Active: 屏蔽后事件仍在活跃表，但 refresh 查询后按 shielded 过滤隐藏
    EventListTimer->>Active: 每 1 秒 refresh() 兜底
    StatusTimer->>Container: 每 1 秒 updateShieldStatus()
    Container->>Bridge: shieldCount()
    opt 宿主另行创建 AlarmLogWidget
        AlarmLogTimer->>AlarmLog: 每 1 秒 refresh() 兜底
        AlarmLog->>Bridge: getActiveEvents()
    end
    Note over Active,Bridge: 配置操作本身不 emit eventsChanged；该信号只由事件通知回调发出
    Note over Active,EM: 若 eventsChanged 在 EventManager 持锁通知中直接触发 refresh，仍会发生默认死锁
```

上图保留活跃列表即时降级/屏蔽路径；配置中心的当前暂存与离开保护路径如下：

```mermaid
sequenceDiagram
    participant User as 用户
    participant Tabs as EventMgrWidget/QTabWidget
    participant Catalog as AlarmCatalogWidget
    participant Bridge as BackendBridge
    participant CFG as ConfigManager
    participant LE as LinkageEngine
    Catalog->>Bridge: getCatalog()
    loop 全目录事件
        Catalog->>Bridge: getEventActionGroups(id, originalLevel)
        Bridge->>LE: getEventActionGroups(...)
        LE->>LE: configMutex_ 内构造两阶段快照
        Bridge-->>Catalog: Qt EventActionGroups
        Catalog->>Catalog: 建立 PendingEventConfig original/current
    end
    User->>Catalog: 跨事件编辑降级/屏蔽/产生/消除
    Note over Catalog: 只改 pendingByEvent_，尚未写后端
    alt Apply
        loop 每个脏事件
            Catalog->>Bridge: 仅写变化的降级/屏蔽
            Bridge->>CFG: 独立 void 写调用
            Catalog->>Bridge: getEventActionGroups(live)
            Bridge->>LE: 单锁 live query
            Note over Catalog: 跳过已删除/换侧，保留未触碰变化
            Catalog->>Bridge: enable/disableAction(..., phase)
            Bridge->>LE: 单次配置锁写入
        end
        Catalog->>Catalog: reload / 恢复仍存在的选择
    else Discard
        Catalog->>Catalog: reloadFromBackend() 丢弃 pending
    else Cancel
        Catalog-->>Tabs: requestLeave=false 或刷新返回
        Tabs->>Tabs: QSignalBlocker 下恢复配置页
        Note over Catalog,Tabs: 保留暂存、选择与状态
    end
    Note over CFG,LE: 多次独立写入，非事务、无回滚、无持久化
```

`dirtyPromptActive_` 阻止消息框嵌套事件循环中的重复确认。“统一应用”只表示一次用户动作遍历全部脏事件，不表示事务提交。

<a id="sequence-async"></a>
## 8. 产生侧与消除侧联动异步执行时序图

### 8.1 产生侧

```mermaid
sequenceDiagram
    participant EM as EventManager
    participant LE as LinkageEngine
    participant M as configMutex_
    participant P as QThreadPool
    participant F as FallbackCallback
    EM->>LE: executeActive(event)
    LE->>M: lock
    LE->>LE: active 默认优先 + 事件列表稳定去重
    LE->>LE: snapshot names + fallback handle
    LE->>M: unlock
    LE->>M: lock
    LE->>LE: 按 active 禁用状态 snapshot callback handles
    LE->>M: unlock
    loop callbacks
        LE-->>P: start(ActionTask(handle))
    end
    LE->>F: 锁外 invoke(eventId,true)
    LE-->>EM: 不等待任务完成
```

### 8.2 消除侧

```mermaid
sequenceDiagram
    participant EM as EventManager
    participant LE as LinkageEngine
    participant M as configMutex_
    participant P as QThreadPool
    participant F as FallbackCallback
    EM->>LE: executeCleared(event)
    LE->>M: lock
    LE->>LE: clear 默认优先 + 事件列表稳定去重
    LE->>LE: snapshot names + fallback handle
    LE->>M: unlock
    LE->>M: lock
    LE->>LE: 按 clear 禁用状态 snapshot callback handles
    LE->>M: unlock
    loop callbacks
        LE-->>P: start(ActionTask(handle))
    end
    LE->>F: 锁外 invoke(eventId,false)
    LE-->>EM: 不等待任务完成
```

### 8.3 共用调度与关闭边界

```mermaid
sequenceDiagram
    participant EM as EventManager
    participant LE as LinkageEngine
    participant Mutex as configMutex_
    participant Pool as "QThreadPool(max 4)"
    participant Task as ActionTask
    participant Callback as "ActionCallback"
    participant Stub as "CmdSender / BuzzerControl"
    participant Fallback as FallbackCallback
    participant Caller as clearAll 调用方

    EM->>LE: executeActive(event) 或 executeCleared(event)
    alt event.originalLevel == Info
        LE-->>EM: 立即返回
        Note over LE,Fallback: 不解析、不入池，也不调用 fallback
    else Active
        LE->>Mutex: lock
        LE->>LE: 先追加 active 等级默认并稳定去重
        alt eventConfig_ 存在 event.id
            LE->>LE: 再追加 configured activeNames
        else 无事件配置
            LE->>LE: 再追加 event.activeActions
        end
        LE->>LE: snapshot names + fallback shared handle
        LE->>Mutex: unlock
        LE->>Mutex: lock
        loop 每个动作名
            LE->>LE: 按 active 禁用状态快照 callback shared handle
            alt 已禁用或名称未注册
                Note over LE: 静默跳过
            else 已注册且未禁用
                LE->>LE: 保存 callback shared handle
            end
        end
        LE->>Mutex: unlock
        loop 每个已快照 callback
            LE-->>Pool: 锁外 start(new ActionTask(callback))
        end
        opt fallback_ 已设置
            LE->>Fallback: 锁外 invoke(id, true)，在调用线程同步执行
        end
        LE-->>EM: 不等待任务完成即返回
    else Cleared
        LE->>Mutex: lock
        LE->>LE: 先追加 clear 等级默认并稳定去重
        alt eventConfig_ 存在 event.id
            LE->>LE: 再追加 configured clearNames
        else 无事件配置
            LE->>LE: 再追加 event.clearActions
        end
        LE->>LE: snapshot names + fallback shared handle
        LE->>Mutex: unlock
        LE->>Mutex: lock
        loop 每个动作名
            LE->>LE: isActionDisabled(id, name, false)
            alt 已注册且未禁用
                LE->>LE: 保存 callback shared handle
            else 已禁用或未注册
                Note over LE: 静默跳过
            end
        end
        LE->>Mutex: unlock
        loop 每个已快照 callback
            LE-->>Pool: 锁外 start(new ActionTask(callback))
        end
        opt fallback_ 已设置
            LE->>Fallback: 锁外 invoke(id, false)，在调用线程同步执行
        end
        LE-->>EM: 不等待任务完成即返回
    end

    par 最多 4 个任务并行
        Pool->>Task: run()
        Task->>Callback: callback_()
        Callback->>Stub: send(...) 或 play(...)
        Note over Callback,Stub: callback 可阻塞；CmdSender 桩等待约 2 秒 ACK
        Stub-->>Task: 返回
        Task-->>Pool: autoDelete
    and 超出并发数的任务
        Note over Pool: 队列继续累积；代码未设置队列上限
    end

    Caller->>LE: clearAll()
    LE->>Pool: waitForDone()
    Note over Caller,Pool: 仅 clearAll 显式等待；阻塞 callback 可使其长期不返回
    Pool-->>LE: 已提交任务全部完成
    LE->>Mutex: lock / 交换清空动作、fallback、配置、默认和禁用集合 / unlock
    Note over LE,Mutex: callback/fallback 句柄在锁外析构
    LE-->>Caller: 返回
```

**解释与风险**：产生和消除侧都按“对应等级默认优先、事件列表随后、首次出现保留”的规则解析。名称/fallback 与 callback/禁用状态分别在两个配置锁临界区快照，因此支持并发读写但不是单一事务版本。任务和 fallback 均在引擎配置锁外调用；上层 `EventManager` 仍持有自身事件锁。线程池没有背压、队列上限、超时、取消或结果聚合。`clearAll()` 仅限调用方已停止新执行并等待在途 `execute*()` 返回的关闭/测试边界。
