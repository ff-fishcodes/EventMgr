# 事件管理中心 — 设计方案

> 版本：v3（修订版）
> v2 → v3 变更：LinkageEngine per-protocolID 注册、前后端兼容适配、notifyFrontend 参数修正

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
| NetworkReceiver | 接收下位机状态帧，解析报警字段，调用本需求 createAlarm / addEvent / clearEvent 接口 |
| Socket 服务端 | 前后端通信，本需求调用其接口推送事件变更通知给前端 |
| LogWriter | 日志写入，本需求调用其接口写入事件日志 |
| 通信监测 | 监测下位机通信状态，断联/恢复通过 createAlarm + addEvent / clearEvent 上报 |
| 管控指令发送 | 向下位机发送管控指令，LinkageEngine 通过注册的 handler 调用其接口 |

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
| 接口风格 | Event 作为值对象入参，ExternalAPI 提供工厂方法 |
| 联动机制 | 回调注册制，Event 携带 `vector<LinkageAction>`，LinkageEngine 分发执行 |
| 并发模型 | 单线程事件循环 |
| 持久化 | 不持久化，重启后从下位机上报重建 |
| 规模 | 中等（5-20 下位机，≤100 状态帧） |
| 防抖 | 不加防抖（信任下位机数据质量） |
| 事件状态 | 产生 / 消除 两状态，不需要确认状态 |
| 告警降级 | 持久降级，精确到 protocolID+frameID+报警字段 |
| 报警屏蔽 | 仅 UI 隐藏，后台继续记录和联动；UI 显示屏蔽计数提示 |
| 降级与屏蔽 | 可独立或同时对同一事件生效 |
| 管控动作 | 由业务在创建 Event 时指定联动内容；LinkageEngine 按注册 handler 执行 |

---

## 3. 整体架构

```
┌──────────────────────────────────────────────────┐
│                    上位机                         │
│                                                  │
│  ┌──────────────┐   本地Socket     ┌────────────┐ │
│  │  前端 (Qt)   │◄───────────────►│ 后端 (C++)  │ │
│  │              │                 │             │ │
│  │ · 事件列表   │                 │ · ExternalAPI│ │
│  │ · 配置管理UI │                 │ · EventManager│ │
│  │ · 颜色分级   │                 │ · LinkageEngine│ │
│  │ · target映射 │                 │ · ConfigManager│ │
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
                    ┌──────────────────────────────────────┐
已有: NetworkReceiver│                                     │ 已有: Socket服务端
      ──────────────►│  ExternalAPI                        ├──────────────► 前端
                     │   ├─ createAlarm()  (工厂方法)       │
已有: 通信监测       │   ├─ addEvent()     (入口)           │  已有: LogWriter
      ──────────────►│   └─ clearEvent()   (入口)           ├──────────────► 日志
                     │                                     │
                     │  EventManager                       │  已有: 管控指令发送
                     │   ├─ 查重 + 存储                     ├──────────────► 下位机
                     │   └─ 调 ConfigManager 计算有效等级   │
                     │                                     │
                     │  LinkageEngine                      │
                     │   ├─ registerHandler()  注册回调     │
                     │   ├─ executeActive()    遍历actions  │
                     │   └─ executeCleared()   分发执行     │
                     │                                     │
                     │  ConfigManager                      │
                     │   ├─ 降级映射                        │
                     │   └─ 屏蔽集合                        │
                     └──────────────────────────────────────┘
```

### 4.2 核心数据结构

事件等级、状态、联动动作和事件值对象统一定义：

```cpp
// ============================
// 事件等级
// ============================
enum class EventLevel {
    Emergency = 1,   // 紧急故障
    Serious   = 2,   // 严重故障
    General   = 3,   // 一般故障
    Info      = 4    // 提示
};

// ============================
// 事件状态
// ============================
enum class EventState { Active, Cleared };

// ============================
// 联动动作
// ============================
struct LinkageAction {
    enum Type {
        LockUI,       // 锁前端控件
        UnlockUI,     // 解锁前端控件
        SendCommand,  // 向下位机发管控指令
        Buzzer,       // 蜂鸣器提示（预留）
        // 后续扩展只需加枚举值，注册新 handler 即可
    };

    Type        type;
    std::string target;   // UI控件标识 / 指令名称 / 蜂鸣器模式名
    std::string param;    // 附加参数（如指令内容、蜂鸣模式）
};

// ============================
// 事件编号
// ============================
using EventId = std::string;  // "protocolID-frameID-报警字段"

// ============================
// 事件值对象
// ============================
struct Event {
    EventId     id;               // "protocolID-frameID-alarmField"
    int         protocolID;
    int         frameID;
    std::string alarmField;
    std::string description;
    EventLevel  originalLevel;    // 业务创建时指定的原始等级
    EventLevel  effectiveLevel;   // 经降级后的实际等级（addEvent 时由 ConfigManager 计算）
    std::vector<LinkageAction> activeActions;   // 告警产生时的联动列表
    std::vector<LinkageAction> clearActions;    // 告警消除时的联动列表
    EventState  state;
};
```

**设计要点：**
- `LinkageAction` 是联动的基本单元。一个事件可携带多个 action，对应"同一故障联动不止一个"
- `target` 是语义标识（如 `"panel_operation"`），不耦合具体的 Qt widget 名
- `activeActions` 和 `clearActions` 由业务在创建 Event 时指定，后续新增联动类型不影响已有代码

---

### 4.3 ExternalAPI（对外接口层）

本需求对外暴露的唯一门面。提供工厂方法和入口方法。

```cpp
class ExternalAPI {
public:
    // ========= 工厂方法 =========

    // 创建告警事件
    // 调用方拿到 Event 后设置联动内容，再调用 addEvent
    Event createAlarm(int protocolID, int frameID,
                      const std::string& alarmField,
                      EventLevel level,
                      const std::string& description);

    // ========= 入口方法 =========

    // 告警产生：入参为 Event 值对象，接口稳定不随 Event 字段变更而改动
    void addEvent(const Event& event);

    // 告警消除：仅需定位键即可
    void clearEvent(int protocolID, int frameID, const std::string& alarmField);
};
```

**设计要点：**
- 断联作为一种事件，与普通告警统一走 createAlarm + addEvent / clearEvent 入口，不单独设接口
- 业务创建 Event 时就应指定等级和联动内容，addEvent 负责查重、降级计算、存储、触发联动、通知

**业务使用示例：**

```cpp
ExternalAPI api;

// 1. 创建事件
Event event = api.createAlarm(1, 3, "temp_high", EventLevel::Emergency, "下位机1-温度过高");

// 2. 指定联动内容
event.activeActions.push_back({LinkageAction::LockUI,      "panel_operation", ""});
event.activeActions.push_back({LinkageAction::SendCommand, "boiler_1",       "emergency_stop"});
event.clearActions.push_back({LinkageAction::UnlockUI,     "panel_operation", ""});

// 3. 添加
api.addEvent(event);
```

---

### 4.4 EventManager（事件管理中心）

核心数据存储，管理所有活跃事件的增删查。

```cpp
class EventManager {
public:
    // 处理告警产生
    void processAddEvent(const Event& event);

    // 处理告警消除
    void processClearEvent(int protocolID, int frameID, const std::string& alarmField);

private:
    // 核心存储：O(1) 查找，验证重复上报
    std::unordered_map<EventId, Event> activeEvents;
};
```

**处理流程（addEvent）：**

```
addEvent(event)
  │
  ▼
查 activeEvents.find(event.id)
  │
  ├── 已存在 → 无操作，直接返回
  │
  └── 不存在:
        │
        ▼
      event.effectiveLevel = ConfigManager.getEffectiveLevel(event.id, event.originalLevel)
      （若有降级配置 → 覆盖为降级后等级；若无 → 保持 originalLevel）
        │
        ▼
      event.state = Active，插入 activeEvents
        │
        ▼
      LinkageEngine.executeActive(event)
        │
        ▼
      if (!ConfigManager.isShielded(event.id)):
        调用 Socket服务端 → 通知前端（含 uiActions）
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
      LinkageEngine.executeCleared(event)
        │
        ▼
      调用 Socket服务端 → 通知前端移除（含 uiActions: unlock_ui 等）
        │
        ▼
      调用 LogWriter → 写日志: "{下位机}-{故障名}-消除"
        │
        ▼
      activeEvents.erase(EventId)
```

---

### 4.5 ConfigManager（配置管理）

管理降级和屏蔽两项配置，两者独立作用可叠加。

```cpp
class ConfigManager {
public:
    // ========= 降级相关 =========
    void setDowngrade(const EventId& id, EventLevel newLevel);
    void removeDowngrade(const EventId& id);

    // 获取有效等级：若有降级配置返回降级后等级，否则返回 originalLevel
    EventLevel getEffectiveLevel(const EventId& id, EventLevel originalLevel);

    // ========= 屏蔽相关 =========
    void setShield(const EventId& id);
    void clearShield(const EventId& id);
    bool isShielded(const EventId& id);
    int getShieldCount();   // 用于前端屏蔽计数提示

private:
    std::unordered_map<EventId, EventLevel> downgradeMap;  // 降级映射
    std::unordered_set<EventId> shieldSet;                  // 屏蔽集合
};
```

**降级与屏蔽叠加效果：**

```
originalLevel（业务创建时指定）
    │
    ▼
降级映射 → effectiveLevel (决定联动行为 + 日志记录)
    │
    ▼
屏蔽检查 → 决定是否通知前端 (屏蔽=不通知, 但联动和日志照常)
```

**初始化：** 每次启动从项目配置模块加载已有配置（无需持久化）。

**UI 操作入口（前端实现，本需求只提供接口）：**
- 事件已产生时：界面提供降级操作按钮
- 事件未产生时：加载所有可能的报警字段，通过复选框设置降级/屏蔽

---

### 4.6 LinkageEngine（联动引擎）

不再按等级 switch，改为**回调注册制**。支持两种注册模式：

- **全局注册** `registerHandler(type, handler)` — 匹配所有 protocolID，用于 LockUI/UnlockUI/Buzzer
- **按设备注册** `registerHandler(type, protocolID, handler)` — 仅匹配指定下位机，用于 SendCommand

分发时先执行全局 handler，再执行匹配 protocolID 的 handler，两者都执行。

```cpp
class LinkageEngine {
public:
    using ActionHandler = std::function<void(const Event&, const LinkageAction&)>;

    // 全局注册：匹配所有 protocolID（LockUI / UnlockUI / Buzzer 用）
    void registerHandler(LinkageAction::Type type, ActionHandler handler);

    // 按 protocolID 注册：仅匹配指定下位机（SendCommand 用，每个下位机单例独立注册）
    void registerHandler(LinkageAction::Type type, int protocolID, ActionHandler handler);

    void executeActive(const Event& event);
    void executeCleared(const Event& event);

private:
    // Key: (actionType, protocolID)，protocolID = -1 表示全局
    using HandlerKey = std::pair<int, int>;
    std::unordered_map<HandlerKey, std::vector<ActionHandler>, KeyHash> handlers_;
};
```

**初始化时注册 handler：**

```cpp
LinkageEngine engine;

// ===== 全局 handler（所有 protocolID 都匹配）=====

engine.registerHandler(LinkageAction::LockUI, [](const Event& e, const LinkageAction& a) {
    SocketServer::notifyFrontend(/* lock_ui JSON */);
});

engine.registerHandler(LinkageAction::UnlockUI, [](const Event& e, const LinkageAction& a) {
    SocketServer::notifyFrontend(/* unlock_ui JSON */);
});

// ===== 按 protocolID 注册（各下位机单例自行注册）=====

// 锅炉控制单例（protocolID=1）
BoilerController::instance().registerWith(engine);
// 内部: engine.registerHandler(SendCommand, 1, [this](auto& e, auto& a) { dispatch(a.target, a.param); });

// 冷却塔控制单例（protocolID=2）
CoolingTowerController::instance().registerWith(engine);
// 内部: engine.registerHandler(SendCommand, 2, ...)
```

**执行流程：**

```
executeActive(event)
  │
  ▼
遍历 event.activeActions:
  对每个 action:
    1. 查 handlers_[(type, -1)]       ← 全局 handler
       → 依次调用
    2. 查 handlers_[(type, protocolID)] ← 设备专属 handler
       → 依次调用
```

**设计要点：**
- 新增联动类型只需：定义新的 `LinkageAction::Type` 枚举值 → 写一个 `registerHandler` 调用。LinkageEngine 本身零改动
- 新增设备控制单例时只需在其 `registerWith()` 中加一行 `registerHandler(SendCommand, myProtocolID, handler)`
- 同一事件多个 action 按注册顺序依次执行
- handler 内调用已有模块接口或直接操作 Qt widget，LinkageEngine 不直接依赖具体模块

### 4.7 前后端部署适配

通过 `FrontendCallbacks` 接口契约，同一套后端核心兼容两种部署模式：

| 模式 | LockUI/UnlockUI 回调 | 事件列表通知回调 |
|------|---------------------|------------------|
| 分离模式 | SocketServer 发送 JSON | SocketServer 发送 JSON |
| 一体模式 | 直接调 Qt widget setEnabled | 直接更新 Qt 事件列表 |

EventManager 构造函数可注入 `NotifyCallback`：有回调走回调，不注入则默认走 SocketServer 桩（向后兼容）。

```cpp
// 分离模式
auto cb = createSocketBasedCallbacks();
EventManager mgr(config, linkage, cb.updateEventList);

// 一体模式
auto cb = createDirectCallbacks();
EventManager mgr(config, linkage, cb.updateEventList);
```

详见 `backend/frontend_adapter.h` 和 `backend/device_controllers_example.h`。

---

## 5. 数据结构设计

### 5.1 事件存储

```cpp
// O(1) 查找：验证重复上报
std::unordered_map<EventId, Event> activeEvents;
```

### 5.2 ConfigManager 存储

```cpp
std::unordered_map<EventId, EventLevel> downgradeMap;  // 降级表
std::unordered_set<EventId> shieldSet;                  // 屏蔽集
```

### 5.3 前端通信协议（本地 Socket）

后端通过已有 Socket 服务端向前端推送事件变更。与 v1 相比增加了 `uiActions` 字段，携带联动目标信息：

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

**前端处理逻辑（Qt 侧伪代码）：**

```cpp
// 前端维护 target 标识 → Qt widget 的映射表
std::map<std::string, QWidget*> targetMap = {
    {"panel_operation",    ui->panelOperation},
    {"btn_start",          ui->btnStart},
    // ...
};

void onSocketMessage(const QJsonObject& msg) {
    if (msg.contains("uiActions")) {
        for (auto& act : msg["uiActions"].toArray()) {
            QString action = act["action"].toString();
            QString target = act["target"].toString();
            QWidget* widget = targetMap[target];
            if (action == "lock_ui")   widget->setEnabled(false);
            if (action == "unlock_ui") widget->setEnabled(true);
        }
    }
}
```

---

## 6. 接口清单汇总

### 6.1 本需求对外提供（ExternalAPI）

| 接口 | 说明 |
|------|------|
| `createAlarm(protocolID, frameID, alarmField, level, desc) → Event` | 工厂：创建事件值对象 |
| `addEvent(event)` | 告警产生（入参为 Event 值对象） |
| `clearEvent(protocolID, frameID, alarmField)` | 告警消除 |

### 6.2 ConfigManager 对外提供

| 接口 | 说明 |
|------|------|
| `setDowngrade(eventId, level)` | 设置降级 |
| `removeDowngrade(eventId)` | 取消降级 |
| `setShield(eventId)` | 设置屏蔽 |
| `clearShield(eventId)` | 取消屏蔽 |
| `getShieldCount()` | 获取屏蔽数量 |

### 6.3 LinkageEngine 对外提供

| 接口 | 说明 |
|------|------|
| `registerHandler(type, handler)` | 注册联动动作处理器 |
| `executeActive(event)` | 执行告警产生侧联动 |
| `executeCleared(event)` | 执行告警消除侧联动 |

### 6.4 本需求对外调用（已有模块接口，桩/伪代码）

| 接口 | 说明 |
|------|------|
| `SocketServer::notifyFrontend(json)` | 向前端推送事件变更（含 uiActions） |
| `LogWriter::write(msg)` | 写日志 |
| `CmdSender::send(protocolID, target, param)` | 向下位机发管控指令 |
| `BuzzerControl::play(target, param)` | 蜂鸣器控制（预留） |

---

## 7. 前端设计要点（Qt）

| 功能点 | 说明 |
|--------|------|
| 事件列表展示 | 接收后端 Socket 推送，实时更新，不同等级不同颜色 |
| 事件格式 | 显示 "{下位机}-{故障描述}" |
| 颜色分级 | 紧急=红色, 严重=橙色, 一般=黄色, 提示=蓝色 |
| 控件锁定 | 收到 `lock_ui` / `unlock_ui` 后按 target 查映射表，对 Qt widget 操作 |
| target 映射 | 前端维护 `target → QWidget*` 映射表，语义标识与控件解耦 |
| 降级操作 | 已产生事件：界面提供降级操作入口 |
| 屏蔽配置 | 未产生事件：加载所有报警字段，复选框设置降级/屏蔽 |
| 屏蔽提示 | 状态栏显示"当前 N 个报警被屏蔽" |
| 清除展示 | 事件消除时从列表中移除 |

---

## 8. 边界与范围

**本需求实现：** ExternalAPI、EventManager、LinkageEngine、ConfigManager

**已有模块（桩/伪代码）：** NetworkReceiver、Socket服务端、LogWriter、通信监测、管控指令发送

---

## 9. 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v1 | 2026-06-17 | 初版：需求分析、模块划分、接口定义 |
| v2 | 2026-06-17 | 修订：Event 值对象入参、ExternalAPI 工厂方法、LinkageEngine 回调注册制、前后端协议增加 uiActions 携带联动 target |
| v3 | 2026-06-17 | 修订：LinkageEngine per-protocolID 注册、前后端兼容适配（FrontendCallbacks）、notifyFrontend 参数 bool 化 |
