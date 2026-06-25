# 事件管理中心 — 架构图

> 基于 v4 设计方案，Mermaid 语言绘制

---

## 1. 类图

```mermaid
classDiagram
    direction TB

    %% ── 数据类型 ──
    class EventLevel {
        <<enumeration>>
        Emergency = 1
        Serious = 2
        General = 3
        Info = 4
    }
    class EventState {
        <<enumeration>>
        Active
        Cleared
    }
    class LinkageAction {
        <<struct>>
        +Type type
        +string target
        +string param
        +int targetProtocolID
    }
    class Event {
        <<struct>>
        +EventId id
        +int protocolID
        +int frameID
        +string alarmField
        +string description
        +EventLevel originalLevel
        +EventLevel effectiveLevel
        +EventState state
        +vector~LinkageAction~ activeActions
        +vector~LinkageAction~ clearActions
    }
    class AlarmDef {
        <<struct>>
        +EventId id
        +string description
        +EventLevel originalLevel
        +bool downgraded
        +EventLevel downgradeTo
        +bool shielded
    }

    %% ── 后端核心 ──
    class ExternalAPI {
        -EventManager& eventMgr_
        -ConfigManager& configMgr_
        +createAlarm(...) Event
        +addEvent(Event)
        +clearEvent(int,int,string)
        +getAlarmCatalog() vector~AlarmDef~
    }
    class EventManager {
        -unordered_map~EventId,Event~ activeEvents_
        -ConfigManager& configMgr_
        -LinkageEngine& linkageEng_
        +processAddEvent(Event)
        +processClearEvent(int,int,string)
        +findEvent(EventId) const Event*
        +activeCount() size_t
    }
    class LinkageEngine {
        -handlers_ : map~(type,protocolID), vector~handler~~
        -eventConfig_ : map~EventId, pair~actions~~
        +registerHandler(Type, handler)
        +registerHandler(Type, protocolID, handler)
        +configureEvent(EventId, active, clear)
        +executeActive(Event)
        +executeCleared(Event)
    }
    class ConfigManager {
        -downgradeMap_ : map~EventId,EventLevel~
        -shieldSet_ : set~EventId~
        +setDowngrade(id, level)
        +removeDowngrade(id)
        +getEffectiveLevel(id, original) EventLevel
        +setShield(id)
        +clearShield(id)
        +isShielded(id) bool
        +getShieldCount() int
    }

    %% ── 桩模块 ──
    class SocketServer {
        <<stub>>
        +notifyFrontend(json)
    }
    class LogWriter {
        <<stub>>
        +write(msg)
    }
    class CmdSender {
        <<stub>>
        +send(protocolID, target, param)
    }
    class BuzzerControl {
        <<stub>>
        +play(target, param)
    }
    class AlarmCatalog {
        <<stub>>
        +getAllDefinitions() vector~AlarmDef~
    }

    %% ── 设备控制器（示例） ──
    class BoilerController {
        <<singleton>>
        +registerWith(LinkageEngine)
        -dispatch(cmd, param)
        -emergencyStop(int reason)
        -reducePower(float mw, int sec)
        -ventSteam()
    }
    class CoolingTowerController {
        <<singleton>>
        +registerWith(LinkageEngine)
        -dispatch(cmd, param)
        -emergencyStop(bool immediate)
        -setFanSpeed(int rpm)
    }

    %% ── 前端 Qt ──
    class EventMgrWidget {
        <<QWidget>>
        -BackendBridge* bridge_
        -QTabWidget* tabs_
        +backend() BackendBridge*
    }
    class EventListWidget {
        <<QWidget>>
        +simulateAddEvent(...)
        +refresh()
    }
    class AlarmCatalogWidget {
        <<QWidget>>
        +loadCatalog()
        +onApply()
    }
    class BackendBridge {
        <<QObject>>
        -ExternalAPI* api_
        -ConfigManager* configMgr_
        +initialize()
        +getCatalog() QVector~CatalogEntry~
        +setDowngrade(id, level)
        +setShield(id)
        +clearShield(id)
        +shieldCount() int
    }

    %% ── 关系 ──
    ExternalAPI --> EventManager : 委托
    ExternalAPI --> ConfigManager : 委托
    ExternalAPI ..> AlarmCatalog : 查询报警定义
    EventManager --> ConfigManager : 查询降级/屏蔽
    EventManager --> LinkageEngine : 触发联动
    EventManager ..> SocketServer : 通知前端
    EventManager ..> LogWriter : 写日志
    LinkageEngine ..> SocketServer : LockUI/UnlockUI handler
    LinkageEngine ..> CmdSender : SendCommand handler
    LinkageEngine ..> BuzzerControl : Buzzer handler
    BoilerController ..> LinkageEngine : registerHandler(SendCommand, 1)
    CoolingTowerController ..> LinkageEngine : registerHandler(SendCommand, 2)
    BackendBridge --> ExternalAPI
    BackendBridge --> ConfigManager
    EventMgrWidget --> BackendBridge
    EventMgrWidget *-- EventListWidget
    EventMgrWidget *-- AlarmCatalogWidget

    Event --> LinkageAction : contains
    Event --> EventLevel
    Event --> EventState
    AlarmDef --> EventLevel
    LinkageAction --> EventLevel
```

---

## 2. 调用关系图

```mermaid
graph TD
    subgraph 外部触发
        A1[NetworkReceiver<br/>下位机状态帧解析]
        A2[通信监测<br/>下位机断联/恢复]
        A3[前端 UI<br/>Qt EventMgrWidget]
    end

    subgraph ExternalAPI
        B1[createAlarm]
        B2[addEvent]
        B3[clearEvent]
        B4[getAlarmCatalog]
    end

    subgraph EventManager
        C1[processAddEvent<br/>查重→降级计算→存储→联动→通知→日志]
        C2[processClearEvent<br/>查id→联动→通知→日志→移除]
    end

    subgraph ConfigManager
        D1[getEffectiveLevel]
        D2[isShielded]
        D3[setDowngrade / setShield]
    end

    subgraph LinkageEngine
        E1[executeActive]
        E2[executeCleared]
        E3[resolveActiveActions<br/>优先查 eventConfig_ 表]
        E4[resolveClearActions<br/>自动 mirror LockUI→UnlockUI]
        E5[dispatchAction<br/>全局handler + per-protocolID handler]
    end

    subgraph 设备控制器
        F1[BoilerController<br/>protocolID=1]
        F2[CoolingTowerController<br/>protocolID=2]
    end

    subgraph 桩模块
        G1[SocketServer<br/>通知前端]
        G2[LogWriter<br/>写日志]
        G3[CmdSender<br/>发管控指令]
    end

    A1 --> B1
    A1 --> B2
    A2 --> B1
    A2 --> B2
    A3 --> B3
    A3 --> B4
    A3 --> D3

    B2 --> C1
    B3 --> C2
    B4 --> D1
    B4 --> D2

    C1 --> E1
    C2 --> E2
    E1 --> E3
    E2 --> E4
    E3 --> E5
    E4 --> E5
    E5 --> F1
    E5 --> F2
    E5 --> G1

    C1 --> D1
    C1 --> D2
    C1 --> G1
    C1 --> G2
    C2 --> G1
    C2 --> G2
    E5 --> G3
```

---

## 3. 时序图

### 3.1 告警产生

```mermaid
sequenceDiagram
    participant NR as NetworkReceiver
    participant API as ExternalAPI
    participant EM as EventManager
    participant CM as ConfigManager
    participant LE as LinkageEngine
    participant BC as BoilerController<br/>(protocolID=1)
    participant SS as SocketServer

    NR->>API: createAlarm(1,3,"temp_high", Emergency, "温度过高")
    API-->>NR: Event对象

    NR->>API: addEvent(event)
    API->>EM: processAddEvent(event)

    Note over EM: 1. 查重
    EM->>EM: activeEvents_.find("1-3-temp_high") → 不存在

    Note over EM: 2. 降级计算
    EM->>CM: getEffectiveLevel("1-3-temp_high", Emergency)
    CM-->>EM: Emergency (无降级配置)

    Note over EM: 3. 存储
    EM->>EM: activeEvents_["1-3-temp_high"] = event

    Note over EM: 4. 触发联动
    EM->>LE: executeActive(event)
    LE->>LE: resolveActiveActions → 查配置表
    Note over LE: {SendCommand(emergency_stop,target=1),<br/>SendCommand(set_fan_speed,target=2),<br/>LockUI(panel_main)}

    LE->>LE: dispatchAction SendCommand(emergency_stop)
    LE->>BC: handler(event, action)
    BC->>BC: dispatch("emergency_stop") → emergencyStop(99)

    LE->>LE: dispatchAction LockUI(panel_main)
    LE->>SS: handler → notifyFrontend({lock_ui, panel_main})
    SS-->>NR: [桩] 打印到 stdout

    Note over EM: 5. 通知前端（未屏蔽）
    EM->>CM: isShielded("1-3-temp_high") → false
    EM->>SS: notifyFrontend({alarm_active, ...})

    Note over EM: 6. 写日志
    EM->>EM: writeLog("下位机1-温度过高-发生")
```

### 3.2 告警消除（自动 mirror UnlockUI）

```mermaid
sequenceDiagram
    participant NR as NetworkReceiver
    participant API as ExternalAPI
    participant EM as EventManager
    participant LE as LinkageEngine
    participant SS as SocketServer

    NR->>API: clearEvent(1, 3, "temp_high")
    API->>EM: processClearEvent(1, 3, "temp_high")

    Note over EM: 1. 查找
    EM->>EM: activeEvents_.find("1-3-temp_high") → 存在

    Note over EM: 2. 触发联动（消除侧）
    EM->>LE: executeCleared(event)
    LE->>LE: resolveClearActions → 查配置表

    Note over LE: 配置表 clearActions = []<br/>但 active 中有 LockUI(panel_main)<br/>→ 自动 mirror: UnlockUI(panel_main)

    LE->>LE: dispatchAction UnlockUI(panel_main)
    LE->>SS: handler → notifyFrontend({unlock_ui, panel_main})

    Note over EM: 3. 通知前端
    EM->>SS: notifyFrontend({alarm_cleared, ...})

    Note over EM: 4. 写日志 + 移除
    EM->>EM: writeLog + erase("1-3-temp_high")
```

### 3.3 前端事前配置（降级/屏蔽）

```mermaid
sequenceDiagram
    participant UI as Qt 前端<br/>AlarmCatalogWidget
    participant BB as BackendBridge
    participant API as ExternalAPI
    participant CM as ConfigManager
    participant AC as AlarmCatalog<br/>(桩)

    Note over UI: 用户打开报警配置页

    UI->>BB: getCatalog()
    BB->>API: getAlarmCatalog()
    API->>AC: getAllDefinitions()
    AC-->>API: [8个报警定义]

    loop 每个 AlarmDef
        API->>CM: hasDowngrade(id) / isShielded(id)
        CM-->>API: 当前配置状态
    end

    API-->>BB: vector<AlarmDef> (含降级/屏蔽状态)
    BB-->>UI: QVector<CatalogEntry>
    Note over UI: 渲染表格，复选框显示当前状态

    Note over UI: 用户勾选降级+屏蔽
    UI->>BB: setDowngrade("1-3-temp_high", Info)
    BB->>CM: setDowngrade("1-3-temp_high", Info)
    UI->>BB: setShield("1-3-temp_high")
    BB->>CM: setShield("1-3-temp_high")

    Note over UI: 用户点击"应用配置"
    UI->>UI: onApply → loadCatalog → 刷新显示
```

### 3.4 降级为提示后报警产生（不联动）

```mermaid
sequenceDiagram
    participant NR as NetworkReceiver
    participant EM as EventManager
    participant CM as ConfigManager
    participant LE as LinkageEngine
    participant SS as SocketServer

    NR->>EM: processAddEvent("1-3-temp_high", Emergency)

    EM->>CM: getEffectiveLevel("1-3-temp_high", Emergency)
    CM-->>EM: Info (前端已降级)

    Note over EM: effectiveLevel = Info<br/>存入 activeEvents_

    EM->>LE: executeActive(event)
    Note over LE: effectiveLevel == Info → return<br/>联动全部跳过！

    EM->>CM: isShielded("1-3-temp_high") → true
    Note over EM: 被屏蔽 → 不通知前端

    Note over EM: 仍然写日志
    EM->>EM: writeLog("下位机1-温度过高-发生")
```

### 3.5 跨设备联动

```mermaid
sequenceDiagram
    participant NR as NetworkReceiver
    participant LE as LinkageEngine
    participant BC as BoilerController<br/>(protocolID=1)
    participant CTC as CoolingTowerController<br/>(protocolID=2)

    NR->>LE: executeActive("1-3-temp_high")
    Note over LE: 配置表:<br/>SendCommand(emergency_stop, target=1)<br/>SendCommand(set_fan_speed, target=2)

    LE->>LE: dispatchAction SendCommand<br/>resolveProtocolID → 1
    LE->>BC: handler(emergency_stop, "99")
    BC->>BC: emergencyStop(99)

    LE->>LE: dispatchAction SendCommand<br/>resolveProtocolID → 2
    LE->>CTC: handler(set_fan_speed, "1200")
    CTC->>CTC: setFanSpeed(1200)

    Note over BC,CTC: 下位机1报故障，下位机2执行动作
```
