# 事件管理中心 — 需求讨论记录

> 本文档记录 EventMgr 项目在需求讨论阶段的所有对话、问题澄清、决策过程，供后续回溯参考。
>
> 讨论日期：2026-06-16 ~ 2026-06-17

---

## 1. 讨论起始

### 1.1 用户提供需求文档

用户将需求文档 `requirment.md` 放置于项目 `doc/` 目录下，要求：
1. 审阅需求文档
2. 指出用户可能未考虑到的点
3. 评估当前需求的合理性

需求文档要点：
- 上位机从多个下位机接收工作状态信息，解析健康信息进行监测管理
- 根据健康状态等级执行管控动作
- 运行平台：飞腾 FT/2000，银河麒麟 V10
- 开发语言：C++11，Qt 5.8.12
- 前后端分离：前端 Qt，后端不依赖 Qt 库
- 上位机与下位机网络通信
- protocolID 区分下位机，frameID 区分状态帧
- 状态帧周期上报，频率 ≥1 Hz
- 功能：告警产生、告警消除、配置管理（降级、屏蔽）

---

## 2. 设计决策澄清（7 个关键问题）

### 2.1 前后端通信机制

**问题：** 前后端分离，后端不使用 Qt 库，前后端之间如何通信？

**选项：**
- A) 本地 Socket（TCP/UDP）
- B) D-Bus（银河麒麟下常见 IPC）
- C) 共享内存 + 消息队列
- D) 不确定

**用户选择：** A — 本地 Socket

**决策理由：** 简单直接，前后端同机部署，本地 Socket 通信可靠且跨语言友好。

---

### 2.2 事件编号生成规则

**问题：** 需求要求"生成事件编号，用于绑定同一个上位机同一种状态帧中的同一个健康状态"。事件编号的生成方式？

**选项：**
- A) 直接使用组合键作为事件 ID — `"{protocolID}-{frameID}-{报警字段名}"` 字符串
- B) 哈希生成数字 ID
- C) 自增序列 + 映射表
- D) 不确定

**用户选择：** A — 组合键字符串

**决策理由：** 简单直观，`protocolID + frameID + 报警字段` 天然唯一确定一个事件，字符串可读性好，便于调试和日志。

---

### 2.3 事件持久化

**问题：** 上位机程序重启后，事件管理中心的已有事件如何处理？

**选项：**
- A) 不持久化 — 重启后清空，从下位机上报重建
- B) 持久化到文件/数据库
- C) 不确定

**用户选择：** A — 不持久化

**决策理由：** 下位机周期性上报（≥1 Hz），重启后自然会重建事件状态，无需引入持久化复杂度。

---

### 2.4 规模预估

**问题：** 下位机数量和状态帧规模预估？

**选项：**
- A) 小规模（2-5 下位机，≤20 状态帧）
- B) 中等规模（5-20 下位机，≤100 状态帧）
- C) 大规模（20+ 下位机，数百状态帧）
- D) 不确定

**用户选择：** B — 中等规模

**决策理由：** 需要合理设计数据结构和查找效率，但不至于过度优化。

---

### 2.5 告警降级的生效范围

**问题：** "为完成某一动作不计代价的场景，需对高等级故障降级"。降级操作的时效性？

**选项：**
- A) 临时降级 — 仅在特定动作执行期间生效，完成后自动恢复
- B) 持久降级 — 用户手动设置后一直生效，直到手动恢复
- C) 两者都需要
- D) 不确定

**用户选择：** B — 持久降级

**决策理由：** 用户手动设置、手动恢复，不自动恢复原等级。

---

### 2.6 报警屏蔽与降级的区别

**问题：** "报警屏蔽与开放"的语义是什么？

**选项：**
- A) 屏蔽 = 完全隐藏，不显示、不记录、不联动
- B) 屏蔽 = 仅 UI 不显示，后台继续记录和联动
- C) 屏蔽和降级是一回事
- D) 不确定

**用户选择：** B — 仅 UI 隐藏，后台照常

**决策理由：** 屏蔽只影响前端展示，后台仍正常记录日志和触发联动。这保证运维可追溯，只是当前不需展示。

**补充确认（用户）：** 降级和屏蔽可独立或同时对同一事件生效。

---

### 2.7 管控动作的具体类型

**问题：** 紧急故障触发的"管控动作"具体包含什么？

**选项：**
- A) 目前只有 UI 锁定/解锁
- B) UI 锁定 + 硬件控制信号（蜂鸣器、指示灯等）
- C) UI 锁定 + 向下位机发送控制指令
- D) 不确定

**用户选择：** C — UI 锁定 + 向下位机发控制指令

**决策理由：** 当前管控动作包含向下位机发指令，其他等级预留接口供后续扩展。

---

## 3. 潜在问题识别与讨论（7 个关注点）

### 3.1 告警产生的防抖

**问题：** 工业场景中信号抖动常见——报警字段可能因瞬态干扰短暂置位后立即恢复，导致"闪报"。

**建议：** 对告警产生增加"连续 N 次置位才确认产生"的去抖动机制。

**用户决定：** 不加防抖，信任下位机数据质量。

---

### 3.2 报警屏蔽的恢复机制

**问题：** 用户屏蔽报警后可能遗忘，导致真正重要的故障被忽略。

**建议：** 屏蔽超时自动恢复，或 UI 上显示"当前有 N 个报警被屏蔽"提示。

**用户决定：** 不需要超时恢复，UI 上有屏蔽计数提示即可。

---

### 3.3 内存数据结构

**问题：** 下位机断连重连后 protocolID 可能不变。是否需要额外的"连接会话"概念？

**用户决定：** 不需要，protocolID 固定不变，断连重连不影响事件编号逻辑。

---

### 3.4 告警降级的作用范围

**问题：** 降级配置作用于所有同名字段的下位机，还是精确到特定下位机的特定字段？

**选项：**
- A) 精确降级 — `protocolID + frameID + 报警字段`
- B) 按报警字段降级 — 所有下位机同名字段一起生效

**用户选择：** A — 精确降级

**决策理由：** 不同下位机的同名字段可能含义不同，精确控制更安全。

---

### 3.5 事件的"确认"状态

**问题：** 工业监控系统中常有"已确认/知晓"状态。是否需要在"产生"和"消除"之间加入？

**用户决定：** 不需要，保持简洁的两状态模型（产生 / 消除）。

---

### 3.6 后端并发模型

**问题：** 中等规模下，后端如何处理多下位机并发上报？

**选项：**
- A) 单线程事件循环
- B) 每连接一线程
- C) 线程池

**用户后续追问：** "如果 10 个下位机，每个 20Hz，一个线程够用吗？"

**分析：**
- 10 × 20Hz = 每秒 200 条消息
- 每条消息处理（哈希查找 + 状态更新 + 联动 + socket write）≈ 微秒级
- 200 条 × 50μs = 10ms/s，CPU 占用约 1%
- 即使扩展到 20 × 50Hz = 1000 条/秒，单线程也无压力

**用户选择：** A — 单线程事件循环

**决策理由：** 事件管理中心本身就是共享状态的汇聚点，单线程天然避免锁竞争。该规模下性能不是瓶颈，代码简洁性和可靠性更重要。耗时 I/O 操作用非阻塞方式处理。

---

### 3.7 下位机通信中断处理

**问题：** 下位机停止上报后，其已有告警事件如何处理？

**选项：**
- A) 不做处理
- B) 超时自动清除 + 产生"通信中断"告警
- C) 超时标记但不自动清除 + 产生"下位机离线"告警

**用户选择：** C — 保留原事件 + 产生离线告警

---

## 4. 模块边界调整

在设计讨论过程中，用户逐步明确了哪些模块已有、哪些属于本需求：

### 4.1 第一轮明确：NetworkReceiver 和 Socket 服务端已有

> "有两个组件我不需要因为是项目里已经有的现成的，一个是 NetworkReceiver 一个是 socket 服务端，这两个组件可以以打桩或者伪代码形式写，这两个模块严格意义上不属于本需求"

### 4.2 第二轮明确：LogWriter 已有

> "还有一个，logwriter 这个组件也是现成的，本需求只需要产生要记录日志的内容，可以调外部接口写日志"

### 4.3 第三轮明确：通信监测（Watchdog）已有

> "watchdog 也不需要本需求管理，因为项目中已有的通信单元会进行监测，监测后会调我们的接口把事件推给我们，我们只要给他们预留接口"

### 4.4 关于断联事件接口的讨论

**用户提问：** "单独给断联事件追加外部接口到底有没有必要呢？因为断联也算是一种事件吧？你觉得哪种好"

**讨论结论：** 断联作为一种事件，与普通告警统一走 `addEvent` / `clearEvent` 入口，不单独设接口。例如：
```
addEvent(protocolID, frameID=0, "device_offline", Serious, "下位机X通信中断")
clearEvent(protocolID, frameID=0, "device_offline")
```

外部接口保持两个核心入口，所有事件类型一视同仁。

### 4.5 管控指令发送已有

> "已有模块。我们用伪代码"

---

## 5. 最终模块划分

### 已有模块（本需求不实现，以桩/伪代码表示）

| 模块 | 说明 |
|------|------|
| NetworkReceiver | 接收下位机状态帧，解析报警字段，调用 ExternalAPI 接口 |
| Socket 服务端 | 前后端通信，本需求调用其接口推送事件变更给前端 |
| LogWriter | 日志写入，本需求调用其接口写入事件日志 |
| 通信监测 | 监测下位机通信状态，断联/恢复通过 addEvent/clearEvent 上报 |
| 管控指令发送 | 向下位机发送管控指令，LinkageEngine 调用其接口 |

### 本需求核心模块

| 模块 | 职责 |
|------|------|
| ExternalAPI | 对外接口层：addEvent、clearEvent |
| EventManager | 事件管理中心：活跃事件存储、增删查、重复判断 |
| ConfigManager | 配置管理：降级映射、屏蔽集合 |
| LinkageEngine | 联动引擎：根据事件等级执行对应管控动作 |

---

## 6. 核心设计决策汇总

| # | 决策项 | 结论 |
|---|--------|------|
| 1 | 前后端通信 | 本地 Socket |
| 2 | 事件编号 | `protocolID-frameID-报警字段` 组合键字符串 |
| 3 | 持久化 | 不持久化，重启后从下位机上报重建 |
| 4 | 规模 | 中等（5-20 下位机，≤100 状态帧） |
| 5 | 告警降级 | 持久降级，精确到 `protocolID+frameID+报警字段` |
| 6 | 报警屏蔽 | 仅 UI 隐藏，后台继续记录和联动 |
| 7 | 管控动作 | UI 锁定/解锁 + 向下位机发控制指令；其他等级预留接口 |
| 8 | 防抖 | 不加防抖（信任下位机数据质量） |
| 9 | 事件状态 | 产生 / 消除两状态，不需要"已确认"状态 |
| 10 | 并发模型 | 单线程事件循环 |
| 11 | 通信中断 | 保留原事件 + 产生"下位机离线"告警 |
| 12 | 降级与屏蔽 | 可独立或同时对同一事件生效 |

---

## 7. 讨论产出

讨论完成后生成了详细设计方案文档：

📄 `docs/superpowers/specs/2026-06-17-event-mgr-design.md`

包含内容：
- 核心设计决策
- 整体架构图
- 4 个核心模块详细设计（ExternalAPI、EventManager、ConfigManager、LinkageEngine）
- 数据结构设计
- 接口清单汇总（对外提供 + 对外调用）
- 前端设计要点
- 边界与范围

---

## 8. 讨论流程总结

```
用户放置需求文档
    │
    ▼
逐项设计决策澄清（7 个问题）
    ├── 前后端通信 → 本地 Socket
    ├── 事件编号 → 组合键
    ├── 持久化 → 不需要
    ├── 规模 → 中等
    ├── 降级 → 持久降级
    ├── 屏蔽 → 仅 UI 隐藏
    └── 管控动作 → UI 锁定 + 下位机指令
    │
    ▼
潜在问题识别（7 个关注点）
    ├── 防抖 → 不加
    ├── 屏蔽恢复 → UI 计数提示即可
    ├── 数据结构 → 哈希表，protocolID 固定
    ├── 降级范围 → 精确降级
    ├── 确认状态 → 不需要
    ├── 并发模型 → 单线程事件循环
    └── 通信中断 → 保留事件 + 离线告警
    │
    ▼
模块边界调整（4 轮澄清）
    ├── NetworkReceiver、Socket 服务端 → 已有
    ├── LogWriter → 已有
    ├── Watchdog/通信监测 → 已有
    └── 管控指令发送 → 已有
    │
    ▼
断联事件接口讨论 → 统一用 addEvent/clearEvent
    │
    ▼
产出设计方案文档
```

---

## 9. 设计方案评审（v1 → v2）

> 讨论日期：2026-06-17

### 9.1 用户提出的三条修改意见

**意见 1：Event 应单独封装，addEvent 入参为 Event**

> "event 应该单独封装，addEvent 入参应该是 event，这样如果 event 本身发生了变更，eventmanager 的接口不用改动。其次应该提供创建 event 的接口。用户业务在创建 event 的时候就应该指定报警等级和报警具体联动内容，然后调用 addEvent。addEvent 在添加时只需要查事件编号"

**分析：** 原设计 `addEvent(protocolID, frameID, alarmField, level, desc)` 有 5 个参数，若 Event 新增字段则接口必改。改为 `addEvent(const Event&)` 后接口永远稳定，Event 内部变更不影响调用方。

**决策：** 采纳。ExternalAPI 提供 `createAlarm()` 工厂方法，业务创建 Event 时即指定等级和联动，`addEvent(event)` 只做查重+存储+触发。

**意见 2：联动不应只有一个 onAlarm 接口，应使用回调注册**

> "同一级别的不同下位机发过来的故障，管控措施是不一样的，且同一故障联动可能不止一个。因此联动的执行不应该只有一个共同的 onAlarm 接口，是否回调注册的形式更合适"

**分析：** 原设计 `onAlarmActive(event)` 内按等级 switch，同等级所有下位机执行相同联动。但实际上不同下位机同等级故障的管控措施不同（如锅炉紧急→停机，冷却塔紧急→降功率），且同一故障可能触发多个联动（锁UI + 发指令 + 蜂鸣器）。

**决策：** 采纳。LinkageEngine 改为回调注册制：
- `registerHandler(LinkageAction::Type, handler)` 注册处理器
- Event 携带 `vector<LinkageAction> activeActions / clearActions`
- `executeActive(event)` 遍历 actions 按类型分发

**意见 3：前端怎么知道锁哪个控件**

> "联动中包含锁 UI，而 UI 又在前端，前端怎么知道这个事件要锁哪个控件"

**分析：** 原设计中后端只发"有紧急事件"的通知，前端不知具体锁哪个控件。这是前后端通信协议的设计缺口。

**决策：** 采纳。LinkageAction 携带 `target` 语义标识（如 `"panel_operation"`），Socket 消息中增加 `uiActions` 数组携带联动目标。前端维护 `target → QWidget*` 映射表，收到消息后按 target 定位控件。

### 9.2 v1 → v2 核心变更

| 变更点 | v1 原设计 | v2 修订后 |
|--------|----------|----------|
| addEvent 入参 | `addEvent(protocolID, frameID, alarmField, level, desc)` — 5 个独立参数 | `addEvent(const Event&)` — Event 值对象 |
| Event 创建 | addEvent 内部构造 Event 对象 | ExternalAPI 提供 `createAlarm()` 工厂，业务创建时指定联动 |
| 联动机制 | LinkageEngine 按 `event.effectiveLevel` switch | 回调注册制，Event 携带 `vector<LinkageAction>`，按 type 分发 |
| LinkageEngine 接口 | `onAlarmActive(event)` / `onAlarmCleared(event)` | `registerHandler()` + `executeActive()` / `executeCleared()` |
| 新增联动类型 | 修改 LinkageEngine switch 代码 | 定义新枚举值 + registerHandler 一行 |
| 前后端协议 | 无 uiActions 字段 | 增加 `uiActions: [{action, target}]` |
| 前端锁控件 | 未定义 | target 语义标识 + 前端 targetMap 映射 |

### 9.3 修订后的模块职责

| 模块 | 职责 |
|------|------|
| ExternalAPI | 对外唯一门面：`createAlarm()` 工厂 + `addEvent()` / `clearEvent()` 入口 |
| EventManager | 活跃事件存储、查重、协调 ConfigManager 计算有效等级 |
| ConfigManager | 降级映射、屏蔽集合（与 v1 一致） |
| LinkageEngine | 回调注册、遍历 Event.actions 分发执行（不再按等级 switch） |

### 9.4 产出

v2 修订版设计文档已更新：

📄 `docs/superpowers/specs/2026-06-17-event-mgr-design.md`（版本 v2）

---

## 10. 设计评审与代码审查（v2 补充）

> 讨论日期：2026-06-17

### 10.1 下位机控制指令分发

**用户疑问：**

> "下位机控制指令并非由一个组件完成，而是每个下位机一个单例组件，每个控制指令的参数类型、数量也不一样，这怎么解决？"

**分析：** 原设计中 `setup.cpp` 为 SendCommand 注册了一个通用 handler，通过 `CmdSender::send(protocolID, target, param)` 发送。这在现实中不可行——不同下位机的控制指令由不同的单例组件负责，参数类型和数量各异（如锅炉 `emergency_stop(int reason)` vs 冷却塔 `emergency_stop(bool immediate)`）。

**决策：** 采纳 per-protocolID 注册模式：

- LinkageEngine 新增 `registerHandler(type, protocolID, handler)` 重载
- 全局 handler（不传 protocolID）：匹配所有 protocolID，用于 LockUI/UnlockUI/Buzzer
- 按 protocolID 的 handler：仅匹配该下位机的事件，用于 SendCommand
- 分发时两者都执行：先全局 handler，再 per-protocolID handler
- 各设备控制单例在初始化时调用 `engine.registerHandler(SendCommand, myProtocolID, handler)`
- 每个单例内部通过 `target` 字段二次分发到不同签名的私有方法

**产出：** `backend/device_controllers_example.h`（BoilerController / CoolingTowerController 示例）

### 10.2 前后端兼容适配

**用户背景：** "我这个项目现阶段还没有前后端分离，能不能在这个基础上兼容？"

**分析：** LinkageEngine 的回调注册机制天然支持两种部署模式的切换。同一套核心模块，只需在初始化时选择不同 handler：
- 分离模式：handler 通过 SocketServer 发送 JSON
- 一体模式：handler 直接调用进程内 Qt widget 方法

**决策：** 引入 `FrontendCallbacks` 接口契约：
- `createSocketBasedCallbacks()` → 前后端分离
- `createDirectCallbacks()` → 前后端一体
- EventManager 构造函数增加可选的 `NotifyCallback` 参数（有回调走回调，无回调走 SocketServer 桩，向后兼容）

**产出：** `backend/frontend_adapter.h`

### 10.3 notifyFrontend 参数修正

**用户指出：**

> "EventManager::notifyFrontend 中 `if (alarmType[0] == 'a')` 这个感觉不太对"

**分析：** 原代码用 `const char* alarmType` 参数，通过首字符判断是 alarm_active（'a'）还是 alarm_cleared（其他）。但 `"alarm_cleared"` 同样以 'a' 开头，导致消除事件时错误地取了 activeActions——这是一个实际 bug。

**决策：** 将参数从 `const char*` 改为 `bool isActive`，内部用三元表达式选择正确的 actions 列表和 JSON type 字段。

### 10.4 v2 补充变更汇总

| 变更点 | 说明 |
|--------|------|
| LinkageEngine per-protocolID 注册 | 新增 `registerHandler(type, protocolID, handler)` 重载，分发时先全局后设备 |
| 设备控制单例 | 各下位机单例自行注册 SendCommand handler，通过 target 字段内部分发 |
| FrontendCallbacks | 分离/一体两种模式通过同一套后端核心切换，仅 handler 注册不同 |
| EventManager NotifyCallback | 构造函数可选注入，不注入则走 SocketServer（向后兼容） |
| notifyFrontend bool 化 | 消除首字符 hack 和 alarm_cleared 取错 actions 的 bug |

---

---

## 11. 设计评审（v3 → v4）：启动时预配置事件联动

> 讨论日期：2026-06-17

### 11.1 事件联动配置时机

**用户指出：**

> "只有事件产生的时候才能去注册联动。但实际每个事件联动什么动作在软件启动阶段就已经确认了"

**分析：** v3 设计中 Event 的 activeActions/clearActions 需在 addEvent 前由调用方 push。问题：
- 职责错位：NetworkReceiver 不该知道"锁什么面板、发什么指令"
- 配置散落：100 个事件的联动分散在各调用点

**决策：** LinkageEngine 新增 `configureEvent(EventId, activeActions, clearActions)` 方法。启动时一次性配置，运行时 addEvent 自动查表，Event 对象无需携带 actions。Event 自带的 actions 保留作为兜底（少数动态场景）。

### 11.2 跨设备联动

**用户提出：**

> "下位机A报了一个故障，有可能需要控制下位机B去执行某个操作"

**分析：** v3 中 SendCommand 分发用 `event.protocolID` 作为路由键，无法跨设备。

**决策：** `LinkageAction` 增加 `targetProtocolID` 字段（0 或未设为与事件源相同）。dispatchAction 对 SendCommand 用 `resolveProtocolID(event, action)` 路由，>0 时发到目标设备。

### 11.3 v3 → v4 核心变更

| 变更点 | 说明 |
|--------|------|
| `LinkageAction::targetProtocolID` | 新增字段，SendCommand 指定目标下位机 |
| `LinkageEngine::configureEvent()` | 启动时预配置事件联动映射表 |
| 执行优先级 | 先查 eventConfig_ 表，找不到才用 Event 自带 actions |
| 路由逻辑 | SendCommand 按 targetProtocolID 路由，支持跨设备 |

---

---

## 12. ActionRegistry 方案设计

> 讨论日期：2026-06-26

### 12.1 问题动机

**用户指出：** 当前每个下位机控制器都要实现 `dispatch(cmd, param)` 做字符串分发，且 `SendCommand` 需要通过 `target` 和 `param` 传递动作名和参数，再由各控制器自行解析。实际上每个业务方法自己知道要什么参数，不需要从外部传。

### 12.2 设计决策

**ActionRegistry 集中注册 + 名字引用：**

- 控制器的 public 方法在 ActionRegistry 中以 lambda 形式注册（参数已被捕获，不需要外部传参）
- 每个能力绑定一个名字：`"(protocolID, name) → callback"`
- `configureEvent` 只引用名字字符串，不再需要 `LinkageAction{type, target, param, targetProtocolID}`
- 跨设备联动通过名字前缀：`"2:fan_high"` = protocolID=2 的 fan_high 动作

**对比结论：** 集中式（ActionRegistry）优于散落式（各控制器 registerWith），原因：可读性强、接入成本低、控制器零依赖。

### 12.3 产出

📄 `docs/superpowers/specs/2026-06-26-action-registry-design.md`

---

---

## 13. ActionRegistry 实施与 UI 解耦

> 日期：2026-06-26

### 13.1 registerAction 简化
移除 protocolID 参数，`registerAction(name, callback)` 只负责声明能力。`configureEvent(id, {names})` 负责绑定。

### 13.2 UI 锁控移出后端
LockUI/UnlockUI 从后端 LinkageEngine 完全移除，清除侧自动 mirror 删除。前端收到 alarm_active/alarm_cleared 后自行处理锁控。

### 13.3 文件清理
删除 `setup.h/cpp`、`frontend_adapter.h`、`device_controllers_example.h`，逻辑迁移至 ActionRegistry。

---

## 14. Tab 页状态同步修复

> 日期：2026-07-05

### 14.1 问题
事件列表页勾选降级/屏蔽后切换到报警配置页，或反向操作，另一页面不会自动刷新，需手动点刷新才能看到最新状态。

### 14.2 根因
- 事件列表的复选框立即写入 ConfigManager，但报警配置页只在构造/点刷新/点应用配置时加载数据
- 报警配置的复选框采用批量应用模式（勾选后点"应用配置"才写入），切 Tab 无任何刷新触发
- 事件列表虽有 1 秒定时器，但切 Tab 时可能尚未触发

### 14.3 决策
- 报警配置页保持批量应用交互（勾选→点"应用配置"按钮生效），不改即时写入
- EventMgrWidget 监听 `QTabWidget::currentChanged` 信号，切 Tab 时立即刷新目标页面
- 事件列表的 1 秒定时器保留（外部 observe 可能随时触发新事件）

### 14.4 实现
改动仅涉及 `eventmgr_widget.h/.cpp`，新增 `onTabChanged(int)` slot，~10 行代码。

---

## 15. 实时报警日志 Widget

> 日期：2026-07-05

### 15.1 需求
主界面需要一个独立的报警日志控件，嵌入到项目主界面固定位置。两列表格：时间 + 报警内容描述（如 "2026-07-04 14:32:05 xxx下位机温度异常"），只显示当前活跃事件。

### 15.2 决策
- 时间戳精确到秒，在 `EventManager::processAddEvent` 自动生成 `YYYY-MM-DD HH:MM:SS` 格式
- AlarmLogWidget 独立可嵌入，构造时传入 `BackendBridge*`，QTimer 每 1 秒轮询
- Event 结构体新增 `std::string timestamp` 字段
- 不改动 ExternalAPI 接口签名
- 颜色按等级：红(紧急)、橙(严重)、黄(一般)、蓝(提示)

### 15.3 新增文件
- `frontend/alarm_log_widget.h/.cpp/.ui` — 独立可嵌入 Widget
- 改动: `event_types.h`, `event_manager.cpp`, `backend_bridge.h/.cpp`, `frontend.pro`

---

## 16. 系统事件与 EventSource 枚举

> 日期：2026-07-05

### 16.1 问题
当前 EventId 格式 `"protocolID-frameID-alarmField"` 只适用于下位机状态帧上报的事件。但后端自身也会监测到事件（通信断连、磁盘满、CPU 过载等），这些事件没有 frameID/alarmField，甚至不关联任何下位机。

### 16.2 决策
- 新增 `EventSource` 枚举：`Device`（下位机上报）/ `System`（后端监测）
- 新增 `SystemEventDef` 结构体 + `system_events.h/.cpp` 预定义列表，业务代码不得自行取名
- Device 事件 ID 保持 `"P-F-A"` 格式不变
- System 事件 ID 直接用事件名（如 `"disk_full"`），关联设备时 `"comm_lost:3"`
- `ExternalAPI::triggerSystemEvent()` 双重载：纯系统事件 + 关联设备系统事件
- `EventManager::processClearEvent(EventId)` 按 ID 精准匹配清除
- `ExternalAPI::clearEvent(string)` 自动解析格式：含两个 '-' 按 Device 处理，否则按 System 处理

### 16.3 预定义事件示例
| 事件名 | 描述 | 等级 |
|--------|------|------|
| comm_lost | 下位机通信断连 | Emergency |
| comm_restore | 下位机通信恢复 | Info |
| disk_full | 磁盘空间不足 | Serious |
| cpu_overload | CPU 过载 | Serious |
| service_error | 关键服务异常 | Emergency |

具体内容由项目方自行增删。

---

## 17. 正式需求与设计文档

> 日期：2026-07-06

### 17.1 背景
按照 `doc/需求提示词.txt` 和 `doc/设计说明提示词.txt` 定义的章节模板，生成符合项目规范的正式文档。

### 17.2 新增文件
- `docs/superpowers/specs/2026-07-06-software-requirements-spec.md` — 需求规格说明文档 (EventMgr-RS-001)，含 RX_xxx 编号体系、能力需求、接口需求、数据需求、设计约束等
- `docs/superpowers/specs/2026-07-06-software-design-spec.md` — 软件设计说明文档 (EventMgr-DS-001)，含 CSC_xxx/CSU_xxx 编号体系、设计决策、体系结构、详细设计、需求可追踪性

### 17.3 说明
- 原有文档不做改动，新文档独立存在
- 新文档采用了正式的章节唯一标识符格式
- 内容基于当前项目实际代码状态编写

---

## 18. 后端事件推送通知优化

> 日期：2026-07-08

### 18.1 讨论
探讨后端引入 Qt 机制是否能优化当前项目。结论：纯 C++11 已经够好，Qt 机制对后端收益不大。但有两项改进值得做：
1. 后端事件变化时主动推送前端（代替纯轮询）
2. 降级/屏蔽配置不必持久化（每次重启恢复原始状态）

### 18.2 决策
- BackendBridge 新增 `eventsChanged()` 信号，EventManager 回调直接 emit
- 前端控件连接信号实现 <1ms 刷新，1 秒定时器保留兜底
- 后端保持纯 C++11 不变，Qt 信号仅在桥接层
- 降级/屏蔽不持久化

### 18.3 改动
- `backend_bridge.h` — 新增 `eventsChanged()` 信号
- `backend_bridge.cpp` — initialize() 传入 NotifyCallback emit 信号
- `eventmgr_widget.cpp` — 连接信号到事件列表 refresh + 屏蔽状态更新
- `alarm_log_widget.cpp` — 连接信号到 refresh

---

## 19. LinkageEngine fallback 机制与 UI 锁控解耦

> 日期：2026-07-08

### 19.1 问题
ActionRegistry 在纯 C++ 层，但 UI 锁控（lock_ui/unlock_ui）需要操作前端控件指针。setup() 中无法访问 UI 对象。

### 19.2 决策
- LinkageEngine 新增 `FallbackCallback`：当动作名称在 actionTable_ 中未注册时，调用 fallback 回调，传入动作名
- BackendBridge 注入 fallback，emit `linkageAction(QString)` 信号
- 宿主项目 connect 此信号，自行处理 UI 锁控逻辑
- ActionRegistry 只注册后端动作（CmdSender/Buzzer），UI 动作名留在 configureEvent 中但不注册

### 19.3 使用方式
```cpp
// 宿主项目
connect(bridge, &BackendBridge::linkageAction,
    this, [this](const QString& name) {
        if (name == "lock_ui:panel_main")   mainPanel_->setEnabled(false);
        if (name == "unlock_ui:panel_main") mainPanel_->setEnabled(true);
    });

// ActionRegistry 中
engine.configureEvent("1-3-temp_high",
    {"cooler_fan", "lock_ui:panel_main"},  // cooler_fan 注册了，lock_ui 走 fallback
    {});
```

### 19.4 改动（初版：per-name fallback）
- `linkage_engine.h/.cpp` — 新增 FallbackCallback + setFallback()，executeNames 查不到时调 fallback
- `backend_bridge.h` — 新增 `linkageAction(QString)` 信号
- `backend_bridge.cpp` — initialize() 注入 fallback emit 信号

### 19.5 改进：per-execute fallback
- 将 fallback 从 per-name（参数为动作名）改为 per-execute（参数为 eventId + isActive）
- fallback 调用位置从 executeNames 移到 executeActive/executeCleared 末尾
- 信号改为 `linkageAction(QString eventId, bool isActive)`
- UI 动作名从 configureEvent 中彻底移除，宿主按 eventId 集中管理 UI 锁控

### 19.6 宿主使用方式（最终版）
```cpp
connect(bridge, &BackendBridge::linkageAction,
    this, [this](const QString& id, bool isActive) {
        if (id == "1-3-temp_high") panelMain_->setEnabled(!isActive);
        if (id == "2-1-vibration") panelMain_->setEnabled(!isActive);
    });
```

*记录完毕。本文档供后续需求回顾和设计决策追溯使用。*

---

## 20. 2026-07-22 后续状态

用户确认需求文档以当前代码为准，并采用分层修订：当前需求基线作为唯一有效需求和验收依据；本文及早期需求、设计继续作为历史讨论记录，不回写早期结论。当前生效入口为[当前需求基线](./2026-07-21-software-requirements-baseline.md)，修订范围与保留规则见[需求文档按当前代码对齐设计](./2026-07-22-requirements-code-alignment-design.md)。
