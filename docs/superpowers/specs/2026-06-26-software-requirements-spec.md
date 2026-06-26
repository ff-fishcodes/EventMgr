# 事件管理中心 — 需求规格说明书

> 版本：v1.1  
> 日期：2026-06-26  
> 状态：已评审

---

## 1. 引言

### 1.1 编写目的

本文档为"事件管理中心"模块的需求规格说明书，定义系统应实现的功能需求、性能需求、接口需求和数据需求，作为设计、开发和测试的依据。

### 1.2 适用范围

本文档适用于飞腾 FT/2000 平台、银河麒麟 V10 操作系统下的上位机事件管理子系统。

### 1.3 术语与缩写

| 术语 | 说明 |
|------|------|
| 上位机 | 本系统，汇聚并管理多个下位机的健康状态 |
| 下位机 | 被监控设备，通过周期状态帧上报健康信息 |
| protocolID | 下位机通信协议标识，唯一区分一个下位机 |
| frameID | 状态帧标识，用于区分同一设备的不同状态帧 |
| 报警字段 | 状态帧中表示特定健康状态的字段（如 temp_high） |
| EventId | 事件编号，格式 `"protocolID-frameID-报警字段"` |
| 联动（Linkage） | 告警产生/消除时自动执行的管控动作序列 |
| 降级（Downgrade） | 降低告警等级的配置操作 |
| 屏蔽（Shield） | 隐藏前端告警展示的配置操作 |
| 等级默认动作 | 按告警等级绑定的全局联动动作 |

### 1.4 参考文档

- 《事件管理需求》`doc/requirment.md`
- 《需求讨论记录》`docs/superpowers/specs/2026-06-17-requirement-discussion-record.md`
- 《设计方案》`docs/superpowers/specs/2026-06-17-event-mgr-design.md`
- 《软件设计说明》`docs/superpowers/specs/2026-06-17-software-design-spec.md`

### 1.5 运行环境

| 项目 | 规格 |
|------|------|
| CPU | 飞腾 FT/2000 |
| 操作系统 | 银河麒麟 V10 |
| 后端语言/标准 | C++11，无 Qt 依赖 |
| 前端开发工具 | Qt 5.15.2 |
| 通信方式 | 上位机-下位机：网络传输；前后端：本地 Socket |

---

## 2. 总体描述

### 2.1 产品视角

事件管理中心是上位机系统的核心子系统，负责接收来自多个下位机的健康状态信息，根据预定义的规则生成告警事件，执行联动管控动作，并将事件状态实时呈现给操作员。

```
下位机1(锅炉) ────┐
下位机2(冷却塔) ──┤  状态帧（周期上报）
下位机3(电网) ────┤  ───────────────►  事件管理中心  ──►  前端 UI
下位机4 ──────────┘                      │
                                          ├──► 日志系统
                                          ├──► 管控指令发送
                                          └──► 蜂鸣器控制
```

### 2.2 用户特征

| 用户类型 | 职责 | 交互方式 |
|----------|------|----------|
| 操作员 | 实时监控下位机健康状态 | 前端事件列表（Tab 1） |
| 配置管理员 | 预设降级/屏蔽策略 | 前端报警配置页（Tab 2） |
| 系统集成开发 | 调用 ExternalAPI 接入报警源 | 编程接口 |

### 2.3 规模假设

- 下位机数量：5-20 台
- 状态帧总数：≤ 100
- 状态帧上报频率：≥ 1 Hz
- 并发模型：单线程事件循环

### 2.4 设计约束

- 前后端分离：前端采用 Qt 框架，后端不依赖 Qt 库
- 已有基础设施（NetworkReceiver、Socket 服务端、LogWriter、通信监测、管控指令发送）不在本需求范围，以桩/伪代码表示
- 事件不持久化，重启后从下位机上报重建

---

## 3. 功能需求

### FR-01：告警产生

#### FR-01.1 事件编号生成
**需求描述：** 系统应为每一个唯一的下位机状态帧报警字段生成全局唯一的事件编号。

**规则：** `EventId = "{protocolID}-{frameID}-{报警字段名}"`

**示例：** `"1-3-temp_high"` 表示下位机1、帧3中的温度过高告警。

#### FR-01.2 重复上报抑制
**需求描述：** 若同一 EventId 的事件已处于活跃状态，再次上报时应静默忽略，不产生新事件、不重复触发联动、不重复通知前端。

#### FR-01.3 事件创建接口
**需求描述：** 系统应提供工厂方法 `createAlarm()` 创建事件值对象，调用方可设置事件的基本属性（protocolID、frameID、报警字段、等级、描述）。联动内容由系统预配置表决定，调用方无需指定。

#### FR-01.4 事件提交接口
**需求描述：** 系统应提供 `addEvent(Event)` 接口，接收事件值对象，完成查重、降级计算、存储、联动触发、前端通知和日志记录。

#### FR-01.5 降级计算
**需求描述：** addEvent 时应自动查询 ConfigManager 的降级配置，计算 effectiveLevel。若有降级配置，effectiveLevel 取降级后等级；否则保持 originalLevel。

#### FR-01.6 屏蔽检查
**需求描述：** addEvent 时应查询 ConfigManager 的屏蔽配置。若事件被屏蔽，跳过前端通知，但联动和日志照常执行。

#### FR-01.7 日志记录
**需求描述：** 事件产生时应写入日志，格式：`"下位机{ID}-{描述}-发生"`。日志通过已有的 LogWriter 模块写入。

---

### FR-02：告警消除

#### FR-02.1 事件消除接口
**需求描述：** 系统应提供 `clearEvent(protocolID, frameID, alarmField)` 接口。根据三个参数构造 EventId，查找活跃事件表。若不存在则静默忽略；若存在则执行消除流程。

#### FR-02.2 消除流程
**需求描述：** 消除流程包括：标记事件为 Cleared 状态 → 触发消除侧联动 → 通知前端移除 → 写消除日志 → 从活跃事件表移除。

#### FR-02.3 消除日志
**需求描述：** 事件消除时应写入日志，格式：`"下位机{ID}-{描述}-消除"`。

---

### FR-03：事件联动

#### FR-03.1 联动动作类型
**需求描述：** 系统应支持以下联动动作类型：

| 动作类型 | 说明 | target 语义 |
|----------|------|-------------|
| LockUI | 锁前端控件，禁止操作员操作 | 控件标识（如 "panel_main"） |
| UnlockUI | 解锁前端控件 | 控件标识 |
| SendCommand | 向下位机发送管控指令 | 指令名称（如 "emergency_stop"） |
| Buzzer | 触发蜂鸣器提示 | 蜂鸣模式名 |

#### FR-03.2 联动配置方式
**需求描述：** 联动动作应在系统启动时完成配置，而非事件产生时逐一指定。支持三层配置：

| 层级 | 方法 | 作用范围 | 优先级 |
|------|------|----------|--------|
| 事件级 | `configureEvent(eventId, active, clear)` | 指定 EventId | 最高 |
| 等级级 | `setLevelDefault(level, actions)` | 所有该等级事件 | 附加 |
| 实例级 | Event 对象的 activeActions/clearActions | 单次事件 | 最低（兜底） |

**运行时合并规则：** 事件级配置 + 等级默认 → 合并执行。若事件级和 Event 自带均无配置，则不做联动。

#### FR-03.3 跨设备联动
**需求描述：** SendCommand 动作应支持通过 `targetProtocolID` 指定目标下位机。值为 0 或不设置时默认为事件来源下位机；值为正整数时路由到指定下位机。

**示例：** 下位机1 报紧急故障，配置 `SendCommand("set_fan_speed", targetProtocolID=2)` 向下位机2 发送提速指令。

#### FR-03.4 LockUI/UnlockUI 自动配对
**需求描述：** 事件消除时应自动将 active 配置中的 LockUI 动作镜像为对应的 UnlockUI 动作，无需在 clear 配置中手动指定。

**示例：** `configureEvent` 的 active 中有 `LockUI("panel_main")`，消除时自动生成 `UnlockUI("panel_main")`。

#### FR-03.5 按等级控制联动
**需求描述：** 当 effectiveLevel 为 Info（提示）时，不执行任何联动动作。其他等级（Emergency/Serious/General）正常执行。

#### FR-03.6 等级默认动作
**需求描述：** 系统应支持按等级设置默认联动动作。所有该等级的事件在产生时自动附加这些动作。

**示例：** 设置 `setLevelDefault(Emergency, {SendCommand("emergency_stop", targetProtocolID=2)})`，则任意下位机报紧急故障时自动向设备2发送停机指令。

---

### FR-04：告警降级

#### FR-04.1 降级设置
**需求描述：** 系统应支持将指定的报警事件降级为更低等级。降级精确到 `protocolID + frameID + 报警字段`。

#### FR-04.2 降级生效
**需求描述：** 降级配置设置后立即生效，影响后续所有该事件的处理。降级不依赖事件是否已产生——事件产生前设置降级，产生时自动生效。

#### FR-04.3 降级影响
**需求描述：** 降级仅改变 effectiveLevel，影响联动决策。若降级为 Info 则不执行联动，但日志和前端通知照常。

#### FR-04.4 降级取消
**需求描述：** 系统应支持取消降级，恢复事件的原始等级。

---

### FR-05：报警屏蔽

#### FR-05.1 屏蔽设置
**需求描述：** 系统应支持对指定事件设置屏蔽。屏蔽精确到 EventId。被屏蔽的事件在产生时不发送前端通知，但联动和日志照常。

#### FR-05.2 屏蔽取消
**需求描述：** 系统应支持取消屏蔽，恢复前端通知。

#### FR-05.3 屏蔽计数
**需求描述：** 系统应提供当前被屏蔽事件的数量，供前端状态栏显示。

---

### FR-06：前端 UI

#### FR-06.1 事件列表页
**需求描述：** 前端应提供活跃事件实时列表，按等级着色（紧急=红色、严重=橙色、一般=黄色、提示=蓝色），显示事件编号、描述、等级、状态。

#### FR-06.2 报警配置页
**需求描述：** 前端应提供报警配置页，展示所有报警定义及其当前降级/屏蔽状态，支持通过复选框设置降级和屏蔽。

#### FR-06.3 报警目录查询
**需求描述：** 系统应提供 `getAlarmCatalog()` 接口，返回所有报警定义的静态信息（编号、描述、原始等级）及当前的降级/屏蔽动态状态。

#### FR-06.4 屏蔽状态提示
**需求描述：** 前端应持续显示当前被屏蔽的报警数量。

#### FR-06.5 嵌入性
**需求描述：** 前端主控件应为 QWidget 子类（EventMgrWidget），可嵌入任意父级 Qt 窗口，不依赖 QMainWindow。

---

### FR-07：外部接口

#### FR-07.1 对外提供接口
| 接口 | 说明 |
|------|------|
| `createAlarm(protocolID, frameID, alarmField, level, desc) → Event` | 创建事件值对象 |
| `addEvent(event)` | 提交告警产生 |
| `clearEvent(protocolID, frameID, alarmField)` | 提交告警消除 |
| `getAlarmCatalog() → vector<AlarmDef>` | 查询报警目录及配置状态 |

#### FR-07.2 对外调用接口（桩）
| 接口 | 说明 |
|------|------|
| `SocketServer::notifyFrontend(json)` | 向前端推送事件变更 |
| `LogWriter::write(msg)` | 写日志 |
| `CmdSender::send(protocolID, target, param)` | 向下位机发管控指令 |
| `BuzzerControl::play(target, param)` | 蜂鸣器控制（预留） |

---

## 4. 非功能需求

### NFR-01：性能
- 事件查重操作 O(1) 时间复杂度（哈希表）
- 单线程事件循环，10 下位机 × 20Hz = 200 msg/s 场景下 CPU 占用 ≤ 5%

### NFR-02：可靠性
- 异常输入（重复上报、消除不存在事件、无效等级）静默处理，不抛异常
- 系统不因单个下位机通信中断而影响其他设备的事件处理

### NFR-03：可扩展性
- 新增联动动作类型：定义枚举值 + 注册 handler，核心引擎零改动
- 新增下位机控制单例：一行 `registerHandler` 即可集成
- 新增前端页面：向 QTabWidget 添加新页即可

### NFR-04：可维护性
- 事件联动配置集中在启动代码中，不散落在各调用点
- 前后端通过回调机制解耦，切换分离/一体模式无需改动核心代码

### NFR-05：平台兼容性
- 后端纯 C++11，仅依赖 STL
- 前端 Qt 5.15.2，C++11 标准

---

## 5. 数据需求

### 5.1 事件存储
```
Key: EventId (std::string) = "protocolID-frameID-alarmField"
Value: Event { id, protocolID, frameID, alarmField, description,
               originalLevel, effectiveLevel, state,
               activeActions[], clearActions[] }
容器: std::unordered_map<EventId, Event>
```

### 5.2 降级配置
```
Key: EventId
Value: 降级后的 EventLevel
容器: std::unordered_map<EventId, EventLevel>
```

### 5.3 屏蔽配置
```
容器: std::unordered_set<EventId>
```

### 5.4 Handler 注册表
```
Key: (LinkageAction::Type, protocolID)  // protocolID=-1 表示全局
Value: std::vector<ActionHandler>
容器: std::unordered_map<std::pair<int,int>, std::vector<handler>>
```

### 5.5 事件联动配置表
```
Key: EventId
Value: (activeActions[], clearActions[])
容器: std::unordered_map<EventId, std::pair<vector, vector>>
```

### 5.6 等级默认动作表
```
Key: EventLevel 的 int 值
Value: activeActions[]
容器: std::unordered_map<int, std::vector<LinkageAction>>
```

### 5.7 前后端通信协议
**告警产生：**
```json
{
    "type": "alarm_active",
    "protocolID": 1,
    "frameID": 3,
    "alarmField": "temp_high",
    "description": "下位机1-温度过高",
    "level": 1,
    "uiActions": [{"action": "lock_ui", "target": "panel_operation"}]
}
```

---

## 6. 变化历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v1.0 | 2026-06-17 | 初始版本，基于需求讨论记录整理 |
| v1.1 | 2026-06-26 | 追加 FR-03.6 等级默认动作、FR-03.4 LockUI 自动配对、FR-03.3 跨设备联动、FR-06.5 嵌入性 |
