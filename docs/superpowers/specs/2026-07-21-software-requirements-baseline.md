# 事件管理中心当前需求基线

> - 文档状态：当前有效、唯一需求与验收基线
> - 代码复核日期：2026-07-23（源码基线 `89dad8b`）
> - 适用范围：当前仓库代码；历史需求和历史需规仅用于追溯

## 1. 文档说明

本文档以截至 2026-07-23 的当前代码中的可验证行为为基线，描述当前实现能力、约束和已知限制，不把历史需求或设计意图写成已实现能力。`doc/requirment.md` 仅作为原始需求来源；与代码不一致时，以本基线的“当前实现”和“原始需求差异”为准。

本次文档分层和历史保留规则见[需求文档按当前代码对齐设计](./2026-07-22-requirements-code-alignment-design.md)。

- **CB-DOC-001**：本基线保存已经确认的需求结论及其源码证据链接，不承担讨论对话、方案比较或决策理由的过程记录。上述过程由 [2026-07-21 文档讨论与验证记录](./2026-07-21-documentation-discussion-record.md) 持续承载，当前交付与验证状态见该记录的[完成后状态与最终验证](./2026-07-21-documentation-discussion-record.md#11-2026-07-22-完成后状态与最终验证)。证据：`docs/superpowers/plans/2026-07-21-documentation-baseline.md`、`docs/superpowers/specs/2026-07-21-documentation-baseline-design.md`、`docs/superpowers/specs/2026-07-21-documentation-discussion-record.md`。
- **CB-DOC-002**：每项基线要求使用唯一的 `CB-` 编号，第 9 章给出可判定的验收方法和具体证据路径。证据：本文第 4、5、6、7、9 章。
- **CB-DOC-003**：本基线不承诺代码未体现的性能、可靠性或持久化能力，也不从局部加锁推导所有组件线程安全。证据：`backend/event_manager.cpp`、`backend/config_manager.cpp`、`backend/linkage_engine.cpp`。
- **CB-DOC-004**：本文件是当前唯一有效的需求与验收基线。`doc/requirment.md`、2026-06-26 和 2026-07-06 需求规格保留为原始输入或历史版本；其中与当前代码不一致的接口、依赖、标识和行为不得用于当前开发或验收。证据：`doc/requirment.md`、`docs/superpowers/specs/2026-06-26-software-requirements-spec.md`、`docs/superpowers/specs/2026-07-06-software-requirements-spec.md`、`docs/superpowers/specs/2026-07-22-requirements-code-alignment-design.md`。

## 2. 系统范围

- **CB-SCOPE-001**：模块接收设备报警位变化和后端系统事件，负责事件构造、活跃事件管理、有效等级计算、屏蔽判断、联动调度、日志调用和前端通知。证据：`backend/external_api.cpp`、`backend/event_manager.cpp`、`backend/config_manager.cpp`、`backend/linkage_engine.cpp`。
- **CB-SCOPE-002**：设备报文接收、协议解析和报警目录的真实配置来源不在当前模块内；当前报警目录由 `AlarmCatalog` 桩提供。证据：`backend/external_api.cpp`、`backend/stubs/alarm_catalog.cpp`。
- **CB-SCOPE-003**：当前可运行形态是前后端同进程：`EventMgrWidget` 创建 `BackendBridge`，桥接层初始化后端单例并直接调用后端接口。证据：`frontend/eventmgr_widget.cpp`、`frontend/backend_bridge.cpp`、`backend/event_mgr_module.cpp`。
- **CB-SCOPE-004**：`SocketServer`、命令发送、蜂鸣器和日志均为桩或回退实现，不代表跨进程服务、真实设备控制或持久化日志已经完成。证据：`backend/stubs/socket_server.cpp`、`backend/stubs/cmd_sender.cpp`、`backend/stubs/buzzer_control.cpp`、`backend/stubs/log_writer.cpp`。

## 3. 运行与依赖约束

- **CB-ENV-001**：项目语言标准为 C++11；生产工程目标为 Qt 5.15.10，启用 Qt Core、Gui、Widgets、Concurrent；两个自动化测试工程另依赖 Qt5Test 5.15.10。构建方式是分别对 `.pro` 执行 `qmake` 后执行 `make`。证据：`frontend/frontend.pro`、`tests/linkage_engine_test.pro`、`tests/alarm_catalog_widget_test.pro`。
- **CB-ENV-002**：后端源码直接依赖 Qt5Core：`EventManager` 和 `ConfigManager` 使用 `QMutex`/`QMutexLocker`，`LinkageEngine` 使用 Qt5Core 中的 `QThreadPool`/`QRunnable`。后端没有调用 `QtConcurrent` API；`frontend.pro` 的 `concurrent` 是完整前端工程声明，不能据此把 Qt5Concurrent 记为后端源码直接依赖。后端最小示例编译应提供 C++11 与 Qt5Core；若演示完整 qmake 工程链接，可由工程文件额外带入 Qt5Concurrent。证据：`backend/event_manager.h`、`backend/event_manager.cpp`、`backend/config_manager.h`、`backend/config_manager.cpp`、`backend/linkage_engine.h`、`backend/linkage_engine.cpp`、`frontend/frontend.pro`。
- **CB-ENV-003**：2026-07-23 Task 8 测试程序报告 QtTest/Qt 5.15.3、GCC 11.3.0；系统编译器命令另报告 GCC 11.4.0。它们是本机 Ubuntu 22.04 x86_64 证据，不替代项目目标 Qt/Qt5Test 5.15.10 或目标平台验证。证据：两个 Qt Test 可执行文件输出及[讨论记录](./2026-07-21-documentation-discussion-record.md)。
- **CB-ENV-004**：飞腾 FT/2000 与银河麒麟 V10 是原始目标平台；仓库没有该目标环境的构建或运行验证证据，因而不能宣称已经兼容验收。证据：`doc/requirment.md`。

## 4. 数据与标识规则

- **CB-ID-001**：设备事件 ID 按 `deviceName-frameID-alarmField` 拼接，例如 `锅炉-3-temp_high`。证据：`backend/external_api.cpp` 中 `createAlarm()`、`backend/event_manager.cpp` 中 `makeEventId()`。
- **CB-ID-002**：纯系统事件 ID 当前等于 `eventName`；关联设备系统事件 ID 等于 `deviceName-0-eventName`。证据：`backend/external_api.cpp` 中两个 `triggerSystemEvent()` 重载。
- **CB-ID-003**：系统事件名必须存在于集中定义表；未知名称只写警告并忽略。证据：`backend/system_events.cpp`、`backend/external_api.cpp`。
- **CB-ID-004**：事件等级数值为 `1=Emergency`、`2=Serious`、`3=General`、`4=Info`；状态为 `Active` 或 `Cleared`，来源为 `Device` 或 `System`。证据：`backend/event_types.h`。
- **CB-ID-005**：设备报警目录未命中时仍创建事件，使用 `Info` 原始等级、报警字段名作为描述，并写目录缺失警告。证据：`backend/external_api.cpp` 中 `triggerAlarm()`。
- **CB-ID-006**：当前 ID 拼接不转义连字符。后端 `ExternalAPI::clearEvent(eventId)` 使用前两个连字符作为分隔符：`deviceName` 含连字符会使分段位置错位，第二个连字符之后的 `alarmField` 或关联设备系统事件 `eventName` 余串则完整保留；中段非法文本经 `std::atoi()` 转换为 `0`。前端 `BackendBridge::triggerAlarm(eventId, ...)` 按全部连字符拆分并要求恰好三段，因此任一组成字段额外含连字符的模拟 ID 都会被拒绝。证据：`backend/external_api.cpp`、`frontend/backend_bridge.cpp`。

## 5. 功能需求

### 5.1 设备告警生命周期

- **CB-FUN-001**：设备产生入口 `triggerAlarm(deviceName, frameID, alarmField, true)` 从报警目录取得原始等级和描述后创建事件。证据：`backend/external_api.cpp` 中 `triggerAlarm()`。
- **CB-FUN-002**：加入事件时按 `EventId` 查重；ID 已存在则静默返回，不更新时间戳，不再次联动、通知或写日志。证据：`backend/event_manager.cpp` 中 `processAddEvent()`。
- **CB-FUN-003**：首次加入时，存储副本的 `effectiveLevel` 由当前降级配置计算。证据：`backend/event_manager.cpp` 中 `processAddEvent()`。
- **CB-FUN-004**：首次加入时，存储副本的状态设为 `Active`。证据：`backend/event_manager.cpp` 中 `processAddEvent()`。
- **CB-FUN-005**：首次加入时生成格式为 `YYYY-MM-DD HH:MM:SS` 的本地时间戳。证据：`backend/event_manager.cpp` 中 `processAddEvent()`。
- **CB-FUN-006**：首次加入的事件副本在联动、通知和事件日志调用之前写入活跃表。证据：`backend/event_manager.cpp` 中 `processAddEvent()`。
- **CB-FUN-007**：产生侧源码逻辑顺序为“写入活跃表、执行产生联动、按屏蔽条件决定是否调用产生通知、调用发生日志”；默认 UI 路径能否完成该顺序受 **CB-LIM-008** 限制。证据：`backend/event_manager.cpp` 中 `processAddEvent()`。
- **CB-FUN-008**：设备清除入口按 `deviceName-frameID-alarmField` 定位事件；不存在时静默返回。证据：`backend/event_manager.cpp` 中三参数 `processClearEvent()`。
- **CB-FUN-009**：设备清除命中时先把活跃表中事件状态设为 `Cleared`。证据：`backend/event_manager.cpp` 中三参数 `processClearEvent()`。
- **CB-FUN-010**：设备清除侧源码逻辑顺序为“设为 Cleared、执行清除联动、调用清除通知、调用消除日志、从活跃表删除”；默认 UI 路径能否完成该顺序受 **CB-LIM-008** 限制。证据：`backend/event_manager.cpp` 中三参数 `processClearEvent()`。

### 5.2 系统事件生命周期

- **CB-FUN-011**：纯系统事件从注册表取得描述和原始等级，设置 `deviceName` 为“系统”，并使用 `eventName` 作为 ID 加入统一生命周期。证据：`backend/external_api.cpp` 中 `triggerSystemEvent(const std::string&, bool)`。
- **CB-FUN-012**：关联设备系统事件从注册表取得描述和原始等级，以 `deviceName-0-eventName` 为 ID 加入统一生命周期。证据：`backend/external_api.cpp` 中带 `deviceName` 的 `triggerSystemEvent()`。
- **CB-FUN-013**：未注册的系统事件名在产生侧和清除侧均只写警告并忽略。证据：`backend/external_api.cpp` 中两个 `triggerSystemEvent()` 重载。
- **CB-FUN-014**：纯系统事件按 `eventName` 清除并调用 `EventManager` 单参数清除；关联设备系统事件的完整 ID 被解析后调用三参数清除。两条命中路径的逻辑顺序均与 **CB-FUN-010** 相同，并受 **CB-LIM-008** 限制。证据：`backend/external_api.cpp` 中 `clearEvent(const std::string&)`、`backend/event_manager.cpp` 中两个 `processClearEvent()` 重载。

### 5.3 活跃事件查询

- **CB-FUN-015**：活跃事件以 `std::unordered_map<EventId, Event>` 保存，`EventId` 为唯一键。证据：`backend/event_manager.h`。
- **CB-FUN-016**：`getActiveEvents()` 在持锁期间复制当前事件并返回值集合；返回顺序不作保证。证据：`backend/event_manager.cpp` 中 `getActiveEvents()`。
- **CB-FUN-017**：系统提供活跃数量和按设备名筛选接口，两者在遍历或读取活跃表时持有 `EventManager` 互斥锁。证据：`backend/event_manager.cpp` 中 `activeCount()`、`findEventsByDeviceName()`。
- **CB-FUN-018**：`findEvent()` 在持锁查找后返回容器元素裸指针，并在函数返回时释放锁；其使用限制见 **CB-LIM-003**。证据：`backend/event_manager.cpp` 中 `findEvent()`。

### 5.4 告警降级

- **CB-FUN-019**：降级配置按 `EventId` 映射目标等级；只接受 1 至 4，越界值静默忽略，取消时删除映射。证据：`backend/config_manager.cpp`。
- **CB-FUN-020**：`effectiveLevel` 只在事件加入时计算；已活跃事件不会因之后修改降级配置而重算。证据：`backend/event_manager.cpp`、`backend/config_manager.cpp`。
- **CB-FUN-021**：联动资格和等级默认动作使用 `originalLevel`，不使用 `effectiveLevel`，因此降级不解除或改变联动。证据：`backend/linkage_engine.cpp` 中 `resolveNamesLocked()`、`executeActive()`、`executeCleared()`。
- **CB-FUN-022**：当前 UI 降级复选框只设置目标等级 `Info(4)`；后端虽接受 1 至 4，界面未提供任意目标等级选择。证据：`frontend/alarm_catalog_widget.cpp`、`frontend/event_list_widget.cpp`、`frontend/backend_bridge.cpp`。

### 5.5 告警屏蔽

- **CB-FUN-023**：`EventManager::processAddEvent()` 的屏蔽分支只控制产生侧主动通知：已屏蔽时跳过 `notifyFrontend(stored, true)`；事件此前已经存储并已提交产生联动，日志语句也不在屏蔽分支内。证据：`backend/event_manager.cpp` 中 `processAddEvent()`。
- **CB-FUN-024**：两个清除处理函数均不检查屏蔽状态，命中事件后都会进入清除通知调用；后续逻辑顺序受 **CB-LIM-008** 限制。证据：`backend/event_manager.cpp` 中两个 `processClearEvent()`。
- **CB-FUN-025**：拉取式 UI 过滤是独立机制：`BackendBridge` 返回屏蔽标志，`EventListWidget` 与 `AlarmLogWidget` 在遍历查询结果时跳过 `shielded=true` 条目。证据：`frontend/backend_bridge.cpp`、`frontend/event_list_widget.cpp`、`frontend/alarm_log_widget.cpp`。

### 5.6 联动动作执行

- **CB-FUN-026**：仅 `originalLevel` 非 `Info` 的事件进入产生侧或清除侧联动执行。证据：`backend/linkage_engine.cpp` 中 `executeActive()`、`executeCleared()`。
- **CB-FUN-027**：产生侧先按原始等级追加等级默认产生动作，再追加事件显式产生动作；运行时没有事件显式配置时改用 `Event::activeActions`。清除侧以相同规则处理各自的清除列表。两侧均保持首次出现顺序并稳定去重。证据：`resolveNamesLocked()`、`appendUnique()`。
- **CB-FUN-028**：等级默认配置由三参数 `setLevelDefault(level, activeActions, clearActions)` 同时定义产生侧与消除侧列表；两侧解析、禁用和查询互不混合。证据：`backend/linkage_engine.h/.cpp`、`backend/action_registry.cpp`。
- **CB-FUN-029**：每个未禁用且已注册动作作为独立 `QRunnable` 提交到最大线程数为 4 的自有 `QThreadPool`；调用方不等待动作完成。证据：`backend/linkage_engine.cpp` 中 `ActionTask`、构造函数、`executeNames()`。
- **CB-FUN-030**：非 `Info` 事件在动作提交循环之后同步调用已配置的 fallback；即使动作列表为空、未注册或全部禁用，fallback 仍调用。证据：`backend/linkage_engine.cpp` 中 `executeActive()`、`executeCleared()`。
- **CB-FUN-031**：当前动作、事件绑定及 `Emergency -> cooler_stop` 等级默认是示例配置，真实控制依赖桩或外部集成。证据：`backend/action_registry.cpp`、`backend/stubs/cmd_sender.cpp`、`backend/stubs/buzzer_control.cpp`。

### 5.7 联动动作禁用

- **CB-FUN-032**：动作禁用的实际字符串键为 `eventId + "|" + actionName`，并分别存入产生侧与清除侧集合；匹配行为以拼接后的键和所选侧集合为准，不承诺对任意字符输入绝对隔离。证据：`backend/linkage_engine.cpp` 中 `makeDisableKey()`、`disableAction()`、`enableAction()`、`isActionDisabled()`。
- **CB-FUN-033**：`getEventActionGroups(eventId, originalLevel)` 在一次 `configMutex_` 临界区内返回产生/消除两组有效动作；每项为 `name`、`displayName`、`enabled`。查询包含等级默认和事件显式配置，不使用运行时 `Event` 兜底字段；缺少显示名时以内部名显示，已配置但无回调的动作仍可查询但不执行。证据：`backend/linkage_engine.cpp` 中 `getEventActionGroups()`、`actionInfosLocked()`、`executeNames()`。
- **CB-FUN-034**：动作禁用集合仅存内存，进程重启或 `clearAll()` 后丢失。证据：`backend/linkage_engine.h`、`backend/linkage_engine.cpp`。

### 5.8 前端展示与配置

- **CB-FUN-035**：主控件提供“事件列表”和“报警配置”页签及屏蔽计数；事件列表显示 ID、描述、时间、有效等级和降级状态，并按等级显示红、橙、黄、蓝颜色。证据：`frontend/eventmgr_widget.cpp`、`frontend/event_list_widget.cpp`、`frontend/event_list_widget.ui`、`frontend/alarm_catalog_widget.ui`。
- **CB-FUN-036**：事件列表每秒拉取一次，并响应 `eventsChanged` 刷新；配置修改本身不发出 `eventsChanged`，页面依靠本地刷新或页签切换加载。证据：`frontend/event_list_widget.cpp`、`frontend/eventmgr_widget.cpp`、`frontend/backend_bridge.cpp`。
- **CB-FUN-037**：报警目录合并全部设备定义和系统事件定义及其降级、屏蔽状态；每个目录事件都暴露降级、屏蔽、联动三个配置域，其中联动域按产生侧和消除侧两个独立列表展示。证据：`ExternalAPI::getAlarmCatalog()`、`AlarmCatalogWidget::reloadFromBackend()`。
- **CB-FUN-038**：前端模拟入口只接受按 `-` 拆分后恰好三段的设备事件 ID，纯系统事件不通过该入口触发。证据：`frontend/backend_bridge.cpp` 中 `triggerAlarm()`。

### 5.9 日志与通知

- **CB-FUN-039**：事件日志调用的文本格式为 `deviceName-description-发生/消除`；目录缺失与未知系统事件另写警告。事件日志调用是否到达受 **CB-LIM-008** 所述默认 UI 通知行为限制。证据：`backend/event_manager.cpp` 中 `writeLog()`、`backend/external_api.cpp`。
- **CB-FUN-040**：通知 JSON 字段为 `type`、`deviceName`、`frameID`、`alarmField`、`description`、`level`；类型为 `alarm_active` 或 `alarm_cleared`，等级取 `effectiveLevel`。证据：`backend/event_manager.cpp` 中 `notifyFrontend()`。
- **CB-FUN-041**：有通知回调时同步调用该回调，否则调用 `SocketServer` 桩；源码中产生通知位于发生日志之前，清除通知位于消除日志和活跃表删除之前。默认 UI 路径的实际结果由 **CB-LIM-008** 定义。证据：`backend/event_manager.cpp` 中 `processAddEvent()`、两个 `processClearEvent()`、`notifyFrontend()`。
- **CB-FUN-042**：目录页为每个事件建立 `PendingEventConfig`，保存后端原始快照及操作者暂存值；各开关变化时立即写入对应暂存值，切换目录行只改变选择并据此渲染目标事件，不写后端。证据：`frontend/alarm_catalog_widget.h/.cpp`。
- **CB-FUN-043**：“应用配置”统一遍历所有脏事件，只提交降级、屏蔽及两侧动作的差异，完成后重新读取后端并恢复仍存在的选中事件。产生侧和消除侧的同名动作分别比较、分别写入。证据：`AlarmCatalogWidget::on_applyBtn_clicked()`、`applyActionDiffs()`。
- **CB-FUN-044**：脏状态下刷新或离开目录页必须提供应用、放弃、取消三种决定；应用提交后继续，放弃重载后端后继续，取消保持暂存状态与选择且阻止离开。嵌套消息循环期间重复请求由 `dirtyPromptActive_` 拒绝。证据：`requestReload()`、`requestLeave()`、`EventMgrWidget::onTabChanged()`。
- **CB-FUN-045**：动作名称区域同时显示用户名称和精确内部名；`ElidedLabel` 按当前 `contentsRect` 右侧省略绘制，完整值保留在 tooltip 与可访问属性中。证据：`frontend/alarm_catalog_widget.cpp`、`tests/test_alarm_catalog_widget.cpp`。
- **CB-FUN-046**：`Info` 原始等级事件的动作查询返回两个空列表，产生/消除执行均不提交动作且不调用 fallback。证据：`LinkageEngine::getEventActionGroups()`、`executeActive()`、`executeCleared()`、`LinkageEngineTest::suppressesInfoActionsAndFallback()`。
- **CB-FUN-047**：联动启用状态按 `eventId + actionName + phase` 隔离；同一动作在不同事件或产生/消除侧的修改互不影响。证据：`disabledActive_`、`disabledClear_` 及 `LinkageEngineTest::isolatesDisableStateByEventActionAndPhase()`。
- **CB-FUN-048**：联动配置页不展示默认/专属来源标签；有效列表只体现默认优先、稳定去重后的最终动作顺序。证据：`renderActionTable()`、`LinkageEngineTest::returnsDefaultsBeforeEventActionsAndDeduplicates()`。

## 6. 非功能约束

- **CB-NFR-001**：`EventManager`、`ConfigManager` 各自保护其容器；`LinkageEngine` 用单个 `configMutex_` 保护动作表、显示名、事件/等级配置、fallback 和两侧禁用集合。联动执行在锁内取得名称、fallback 和不可变回调句柄快照，锁外提交任务和调用用户回调；单次分组查询在同一锁内完成。证据：三个管理器头/实现及并发测试。
- **CB-NFR-002**：联动动作线程池最大工作线程数为 4；本基线不规定响应时间、吞吐量、队列上限或完成时限。证据：`backend/linkage_engine.cpp`。
- **CB-NFR-003**：后端事件接口使用 C++ 标准类型，桥接层转换为 Qt 类型；同进程全局接口要求先执行 `EventMgrModule::init()`。证据：`backend/event_types.h`、`backend/event_mgr_module.cpp`、`frontend/backend_bridge.cpp`。
- **CB-NFR-004**：不得把当前后端描述为无 Qt 依赖，也不得把 Socket 打印桩描述为已验证的跨进程可靠通信。证据：`backend/event_manager.h`、`backend/config_manager.h`、`backend/linkage_engine.h`、`backend/stubs/socket_server.cpp`。

## 7. 当前限制

- **CB-LIM-001（事件 ID 分隔符）**：设备 ID 创建和解析依赖 `-`。后端清除按前两个连字符切分，前端模拟入口要求恰好三段；设备名或字段名含连字符时语义不一致，帧号转换也缺少严格格式校验。证据：`backend/external_api.cpp`、`frontend/backend_bridge.cpp`。
- **CB-LIM-002（系统 ID 注释）**：部分注释把 ID 写为“系统-0-eventName”，但纯系统事件的实际代码使用 `eventName`；集成以 **CB-ID-002** 为准。证据：`backend/event_types.h`、`backend/external_api.cpp`。
- **CB-LIM-003（查找指针生命周期）**：`findEvent()` 返回容器元素裸指针后立即释放互斥锁。`unordered_map` 重哈希本身不会使元素指针或引用失效；实际失效条件包括该元素被 `erase`、容器被销毁。锁释放后若其他线程同时访问或修改相关对象，调用方还存在未同步并发访问风险。证据：`backend/event_manager.cpp` 中 `findEvent()`、`processClearEvent()`，以及 C++11 `std::unordered_map` 元素失效规则。
- **CB-LIM-004（联动快照边界）**：`LinkageEngine` 已消除其配置容器的无锁读写，但执行分两次线性化：先快照名称/fallback，再按当时禁用状态快照回调。两次锁之间的并发配置可使一次执行观察到两个配置时点；这是 best-effort 快照，不是跨调用事务。证据：`executeActive()`、`executeCleared()`、`executeNames()`。
- **CB-LIM-005（模块生命周期）**：`EventMgrModule` 使用 `new` 创建全局单例，无关闭、释放流程或使用前空值保护，资源持续到进程退出。证据：`backend/event_mgr_module.h`、`backend/event_mgr_module.cpp`。
- **CB-LIM-006（JSON 转义）**：通知 JSON 由字符串流手工拼接，没有转义引号、反斜线或控制字符。证据：`backend/event_manager.cpp` 中 `notifyFrontend()`。
- **CB-LIM-007（内存状态）**：活跃事件、降级、屏蔽和动作禁用配置仅存内存，进程重启后丢失。证据：`backend/event_manager.h`、`backend/config_manager.h`、`backend/linkage_engine.h`、`backend/event_mgr_module.cpp`。
- **CB-LIM-008（默认 UI 通知死锁）**：当前默认同线程接线已满足死锁条件。`processAddEvent()` 和两个 `processClearEvent()` 持有非递归 `QMutex` 调用 `notifyFrontend()`；桥接回调同步发出 `eventsChanged`，默认连接直接进入 `EventListWidget::refresh()`，再经 `BackendBridge::getActiveEvents()` 调用 `EventManager::getActiveEvents()`，在读取前重入同一互斥锁。因此未屏蔽事件的产生通知会卡住，所有已命中事件的清除通知也会卡住；产生侧后续发生日志、清除侧后续消除日志和删除无法执行。跨线程发出或排队连接可能不同，但不是当前默认同线程 UI 路径。证据：`backend/event_manager.h`、`backend/event_manager.cpp`、`frontend/backend_bridge.cpp`、`frontend/eventmgr_widget.cpp`、`frontend/event_list_widget.cpp`。
- **CB-LIM-009（桩能力）**：Socket、日志、报警目录、命令发送和蜂鸣器尚未提供生产级持久化、错误恢复、鉴权、重连、确认或真实设备适配。证据：`backend/stubs/socket_server.cpp`、`backend/stubs/log_writer.cpp`、`backend/stubs/alarm_catalog.cpp`、`backend/stubs/cmd_sender.cpp`、`backend/stubs/buzzer_control.cpp`、`backend/action_registry.cpp`。
- **CB-LIM-010（动作禁用键碰撞）**：禁用键没有结构化编码或转义；若允许 `eventId` 或 `actionName` 包含 `|`，不同输入对可能拼出相同的 `eventId|actionName` 字符串。当前实现隐含两部分不包含可造成歧义的 `|` 字符这一假设。证据：`backend/linkage_engine.cpp` 中 `makeDisableKey()`。
- **CB-LIM-011（配置应用非事务）**：目录页对降级、屏蔽和每个动作使用多个 void 写接口，没有事务或回滚。动作写入前的 live query 会跳过已删除或已换侧动作并保留未触碰动作的并发变化，但查询与后续逐项写入之间仍有竞态窗口；部分写入失败或进程中断时不保证全有或全无。证据：`AlarmCatalogWidget::on_applyBtn_clicked()`、`applyActionDiffs()`。

## 8. 原始需求差异

| 原始要求 | 当前实现 | 影响 | 状态 |
|---|---|---|---|
| 开发工具为 Qt 5.8.12 | 项目声明 Qt 5.15.10；2026-07-21 本机验证为 Qt 5.15.3 | 历史版本、项目基线和验证环境必须分开管理 | 已偏离 |
| 后端不采用 Qt 库 | 后端直接使用 Qt5Core 的 `QMutex`、`QMutexLocker`、`QThreadPool`、`QRunnable`；没有直接使用 QtConcurrent API | 后端源码最小构建需要 Qt5Core | 已偏离 |
| 通过 `protocolID + frameID` 标识设备状态帧 | 当前事件 ID 使用 `deviceName-frameID-alarmField` | 旧标识不能直接互操作 | 已变更 |
| 降级用于忽视严重故障并解除管控 | 降级改变有效等级和显示，联动仍用原始等级 | 降级不解除当前联动 | 未满足原语义 |
| 历史设计曾描述单线程或串行处理 | 活跃表、配置与联动配置分别使用互斥锁，联动动作由最多 4 线程异步执行 | 动作并发且顺序不确定；联动一次执行采用两个线性化点的 best-effort 快照 | 当前为并发实现 |
| 前后端通信并考虑分离 | `BackendBridge` 同进程直接调用；`SocketServer` 仅为打印桩 | 不能验收真实分进程部署 | 尚未完成分离 |
| 屏蔽控制界面展示 | 产生主动通知由后端屏蔽分支控制；拉取视图另行过滤；清除通知不检查屏蔽 | 屏蔽是两层 UI 可见性策略，不停用事件处理 | 部分满足并存在通知缺陷 |
| 事件日志供问题排查 | `LogWriter` 只输出标准输出，日志页展示活跃事件而非历史日志 | 无持久化审计记录 | 桩实现 |

## 9. 验收追踪

| 验收主题 | 需求编号 | 可判定检查 | 主要证据 |
|---|---|---|---|
| 基线文档职责 | CB-DOC-001, CB-DOC-002, CB-DOC-003, CB-DOC-004 | 检查结论均有编号和证据，讨论理由链接到计划记录，且无无依据性能承诺；确认本文件是当前有效且唯一的需求与验收基线，`doc/requirment.md` 标为非当前原始输入，2026-06-26/2026-07-06 需求规格标为历史归档，`docs/README.md` 明确当前需求基线是唯一有效的需求与验收文档并给出历史失效标签 | 本文、`doc/requirment.md`、`docs/README.md`、`docs/superpowers/specs/2026-06-26-software-requirements-spec.md`、`docs/superpowers/specs/2026-07-06-software-requirements-spec.md`、`docs/superpowers/plans/2026-07-21-documentation-baseline.md` |
| 模块边界 | CB-SCOPE-001, CB-SCOPE-002 | 从门面到事件、配置、联动逐项追踪；确认接收/解析未实现且目录来自桩 | `backend/external_api.cpp`、`backend/stubs/alarm_catalog.cpp` |
| 部署与桩边界 | CB-SCOPE-003, CB-SCOPE-004 | 启动控件核对直接调用；检查 Socket、命令、蜂鸣器、日志桩没有真实外部集成 | `frontend/backend_bridge.cpp`、`backend/event_mgr_module.cpp`、`backend/stubs/socket_server.cpp`、`backend/stubs/cmd_sender.cpp`、`backend/stubs/buzzer_control.cpp`、`backend/stubs/log_writer.cpp` |
| 项目构建声明 | CB-ENV-001 | 检查 C++11、Qt/Qt5Test 5.15.10 目标模块和三个 qmake/make 入口 | `frontend/frontend.pro`、`tests/linkage_engine_test.pro`、`tests/alarm_catalog_widget_test.pro` |
| 后端 Qt 依赖 | CB-ENV-002, CB-NFR-004 | 搜索后端 Qt 类型和 QtConcurrent 调用；最小后端仅按 Qt5Core 编译，完整工程按 `.pro` 构建 | `backend/event_manager.h`、`backend/event_manager.cpp`、`backend/config_manager.h`、`backend/config_manager.cpp`、`backend/linkage_engine.h`、`backend/linkage_engine.cpp`、`frontend/frontend.pro` |
| 本机环境记录 | CB-ENV-003 | 核对 Qt Test 启动信息中的 QtTest/Qt 5.15.3、GCC 11.3.0，并与系统 GCC 11.4.0 和目标 5.15.10 分开记录 | 两个 Qt Test 可执行文件输出、讨论记录 |
| 目标平台边界 | CB-ENV-004 | 检查原始平台要求，并确认仓库无目标平台验证产物 | `doc/requirment.md` |
| 事件 ID | CB-ID-001, CB-ID-002, CB-ID-006 | 分别构造设备、纯系统和关联设备系统事件并比对精确 ID；验证设备名含连字符时后端分段错位，后缀含连字符时后端保留第二个连字符后的余串，非法中段经 `std::atoi()` 转为 `0`，前端模拟入口对拆分后多于三段的 ID 拒绝处理 | `backend/external_api.cpp`、`backend/event_manager.cpp`、`frontend/backend_bridge.cpp` |
| 系统名与类型 | CB-ID-003, CB-ID-004 | 枚举定义和值；用未知系统名验证只写警告 | `backend/event_types.h`、`backend/system_events.cpp`、`backend/external_api.cpp` |
| 目录缺失回退 | CB-ID-005, CB-FUN-001 | 触发目录内外设备事件，比对等级、描述和警告 | `backend/external_api.cpp` |
| 设备加入幂等 | CB-FUN-002 | 同 ID 连续加入两次，核对时间戳和副作用只发生一次 | `backend/event_manager.cpp` |
| 设备加入字段 | CB-FUN-003, CB-FUN-004, CB-FUN-005 | 设置降级后加入，分别核对有效等级、Active 状态和时间戳格式 | `backend/event_manager.cpp`、`backend/config_manager.cpp` |
| 设备加入顺序 | CB-FUN-006, CB-FUN-007 | 用可观测回调核对入表先于联动且源码顺序符合要求；默认 UI 场景按限制项验收 | `backend/event_manager.cpp` |
| 设备清除定位与状态 | CB-FUN-008, CB-FUN-009 | 清除不存在 ID 应无副作用；命中时联动回调观察到 Cleared | `backend/event_manager.cpp` |
| 设备清除顺序 | CB-FUN-010 | 用桩记录清除联动、通知、日志、删除顺序；默认 UI 场景按限制项验收 | `backend/event_manager.cpp` |
| 系统事件产生 | CB-FUN-011, CB-FUN-012, CB-FUN-013 | 触发纯系统、关联设备和未知名称，核对 ID、字段及忽略行为 | `backend/external_api.cpp`、`backend/system_events.cpp` |
| 系统事件清除 | CB-FUN-014 | 分别按两类 ID 清除，核对纯系统走单参数、关联设备走三参数清除及其顺序 | `backend/external_api.cpp`、`backend/event_manager.cpp` |
| 活跃存储与集合查询 | CB-FUN-015, CB-FUN-016 | 加入不同 ID 和重复 ID，核对唯一键、返回值拷贝和不依赖顺序 | `backend/event_manager.h`、`backend/event_manager.cpp` |
| 活跃统计与设备查询 | CB-FUN-017 | 核对数量和按设备名筛选，并静态检查持锁范围 | `backend/event_manager.cpp` |
| 单项查找接口 | CB-FUN-018, CB-LIM-003 | 检查返回后锁已释放；分别验证元素删除、容器销毁和并发访问风险说明，不把重哈希列为失效原因 | `backend/event_manager.cpp`、C++11 容器规则 |
| 降级配置 | CB-FUN-019, CB-FUN-020 | 覆盖 0、1、4、5 及取消；活跃后改配置应不重算存储值 | `backend/config_manager.cpp`、`backend/event_manager.cpp` |
| 降级联动与 UI | CB-FUN-021, CB-FUN-022 | 对降级非 Info 原始事件核对联动不变；检查 UI 只写目标 4 | `backend/linkage_engine.cpp`、`frontend/alarm_catalog_widget.cpp`、`frontend/event_list_widget.cpp` |
| 产生屏蔽分支 | CB-FUN-023 | 对屏蔽事件产生，核对入表和联动发生、产生通知跳过、日志语句不在条件块 | `backend/event_manager.cpp` |
| 清除屏蔽分支 | CB-FUN-024 | 静态确认两个清除路径均无屏蔽判断且进入通知；默认 UI 结果按限制项验收 | `backend/event_manager.cpp` |
| 拉取式过滤 | CB-FUN-025 | 让桥接返回屏蔽项，核对事件列表和日志视图均跳过该项 | `frontend/backend_bridge.cpp`、`frontend/event_list_widget.cpp`、`frontend/alarm_log_widget.cpp` |
| 联动资格与名称 | CB-FUN-026, CB-FUN-027, CB-FUN-028 | 覆盖 Info/非 Info、事件配置、自带动作、等级默认、重复名和未知名 | `backend/linkage_engine.cpp` |
| 联动调度与 fallback | CB-FUN-029, CB-FUN-030 | 核对线程池上限；分别用空、未知、禁用和正常动作验证 fallback | `backend/linkage_engine.cpp` |
| 示例动作边界 | CB-FUN-031 | 检查注册表及控制桩，确认无真实设备实现 | `backend/action_registry.cpp`、`backend/stubs/cmd_sender.cpp`、`backend/stubs/buzzer_control.cpp` |
| 动作禁用键 | CB-FUN-032, CB-LIM-010 | 比对精确拼接键和侧集合；构造含 `|` 的两组输入验证可碰撞 | `backend/linkage_engine.cpp` |
| 动作查询与存储 | CB-FUN-033, CB-FUN-034, CB-FUN-046, CB-FUN-047, CB-FUN-048 | 用后端 Qt Test 核对默认优先稳定去重、产生/消除隔离、Info 空组、缺回调仍可查询及每事件/动作/阶段禁用；静态检查 `clearAll()` 的清空范围 | `build-tests/linkage/test_linkage_engine`、`tests/test_linkage_engine.cpp`、`backend/linkage_engine.cpp` |
| 主界面展示 | CB-FUN-035 | 检查页签、字段、计数和四级颜色 | `frontend/eventmgr_widget.cpp`、`frontend/event_list_widget.cpp`、`frontend/event_list_widget.ui`、`frontend/alarm_catalog_widget.ui` |
| 前端刷新 | CB-FUN-036 | 观察一秒定时器、事件信号和配置后的本地刷新路径 | `frontend/event_list_widget.cpp`、`frontend/eventmgr_widget.cpp` |
| 配置目录与暂存应用 | CB-FUN-037, CB-FUN-042, CB-FUN-043, CB-FUN-044, CB-FUN-045, CB-LIM-011 | 用前端 Qt Test 核对全目录三类配置域、两阶段列表、跨事件暂存、差异应用、live membership best-effort、刷新/离开三决策、非事务边界和省略文本完整身份 | `build-tests/catalog/test_alarm_catalog_widget`、`tests/test_alarm_catalog_widget.cpp` |
| 响应式截图 | CB-FUN-045, CB-NFR-003 | 在 1280x760 与 760x720 下检查无重叠、动作身份可见、DPR 像素采样非空；分别运行缩放 1/2 与字体 DPI 120/144 | `AlarmCatalogWidgetTest::capturesResponsiveScreenshots()`、`build-tests/screenshots/` |
| 模拟入口 | CB-FUN-038, CB-LIM-001 | 输入三段、少段、多段、非数字帧号和含连字符字段，核对当前解析边界 | `frontend/backend_bridge.cpp`、`backend/external_api.cpp` |
| 日志格式 | CB-FUN-039 | 用日志桩捕获产生、消除、目录缺失和未知系统名文本 | `backend/event_manager.cpp`、`backend/external_api.cpp`、`backend/stubs/log_writer.cpp` |
| 通知载荷与顺序 | CB-FUN-040, CB-FUN-041 | 捕获回调和 Socket 回退，逐字段比对 JSON，并静态核对通知相对日志/删除位置 | `backend/event_manager.cpp`、`backend/stubs/socket_server.cpp` |
| 锁、回调所有权与线程池边界 | CB-NFR-001, CB-NFR-002, CB-LIM-004 | 静态检查三个管理器持锁范围、`shared_ptr<const callback>` 锁内快照/锁外析构与调用、两个线性化点和联动池最大线程数；运行并发与重入 fallback 测试 | `backend/linkage_engine.h/.cpp`、`LinkageEngineTest` |
| 类型与初始化 | CB-NFR-003 | 在初始化前后检查单例入口，核对桥接类型转换 | `backend/event_mgr_module.cpp`、`frontend/backend_bridge.cpp` |
| ID 注释差异 | CB-LIM-002 | 对比注释和赋值语句，确认纯系统 ID 以实际赋值为准 | `backend/event_types.h`、`backend/external_api.cpp` |
| 联动 best-effort 快照 | CB-LIM-004 | 并发配置、查询和执行应无数据竞争/死锁；同时确认名称/fallback 与回调/禁用状态分属两个线性化点，不宣称跨调用事务 | `backend/linkage_engine.h/.cpp`、`LinkageEngineTest::supportsConcurrentConfigurationQueryAndExecution()` |
| 模块生命周期 | CB-LIM-005 | 连续初始化并检查只创建一次；确认无关闭、释放和空值保护接口 | `backend/event_mgr_module.h`、`backend/event_mgr_module.cpp` |
| JSON 转义风险 | CB-LIM-006 | 使用引号、反斜线和控制字符构造字段，验证输出不是可靠转义 JSON | `backend/event_manager.cpp` |
| 内存状态 | CB-LIM-007 | 检查无保存/加载路径，并验证 `clearAll()` 或重启后状态消失 | `backend/event_manager.h`、`backend/config_manager.h`、`backend/linkage_engine.h` |
| 默认 UI 通知死锁 | CB-LIM-008 | 用受控超时在同线程触发未屏蔽产生和已命中清除，确认停在 `getActiveEvents()` 重入锁；另核对屏蔽产生跳过通知 | `backend/event_manager.cpp`、`frontend/backend_bridge.cpp`、`frontend/eventmgr_widget.cpp`、`frontend/event_list_widget.cpp` |
| 桩能力限制 | CB-LIM-009 | 逐个检查 Socket、日志、目录、命令和蜂鸣器没有生产集成 | `backend/stubs/socket_server.cpp`、`backend/stubs/log_writer.cpp`、`backend/stubs/alarm_catalog.cpp`、`backend/stubs/cmd_sender.cpp`、`backend/stubs/buzzer_control.cpp`、`backend/action_registry.cpp` |

Task 8 本机证据为：后端 Qt Test **12 passed / 0 failed**，前端 Qt Test **19 passed / 0 failed**；截图槽在 `QT_SCALE_FACTOR=1`、`QT_SCALE_FACTOR=2`、`QT_FONT_DPI=120`、`QT_FONT_DPI=144` 四种运行下均为 **3 passed / 0 failed**。这些结果只覆盖 Ubuntu 22.04 x86_64、QtTest/Qt 5.15.3 和测试程序报告的 GCC 11.3.0，不替代目标 Qt/Qt5Test 5.15.10 或目标平台验收。

本基线验收通过的最低条件是：全部已定义 `CB-` 编号都在本章具有明确检查和证据，当前行为可由源码或受控测试复现，且不得把第 7 章限制或第 8 章未满足项表述为已经完成。
