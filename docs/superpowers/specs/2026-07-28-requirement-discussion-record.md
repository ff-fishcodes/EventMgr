# 需求讨论记录（2026-07-28）

## 联动项配置开关归属

### 用户需求

联动项开关属于事件配置，应提供外部接口，而不是由前端直接操作联动执行引擎的内部状态。

### 现状识别

- 降级、屏蔽由 `ConfigManager` 管理。
- 联动开关由 `LinkageEngine` 的产生侧、消除侧禁用集合管理。
- `BackendBridge` 直接调用 `LinkageEngine::enableAction()` / `disableAction()`。
- `ExternalAPI` 只暴露动作注册、事件联动定义和等级默认定义，没有联动开关设置/查询。

### 方案讨论

讨论三个方案：

1. 联动开关归 `ConfigManager`，通过 `ExternalAPI` 设置/查询；推荐。
2. 状态留在 `LinkageEngine`，仅由 `ExternalAPI` 转发；改动小但职责仍分散。
3. 开关嵌入 `AlarmDef`；动态、分阶段、多动作配置会污染定义结构，不采用。

### 用户确认

用户选择方案 1：联动开关归 `ConfigManager`。当前保持内存配置，与降级和屏蔽一致；不在本次增加持久化。

### 设计结论

- `ConfigManager` 以 EventId、动作名、阶段为维度保存关闭项，默认启用。
- `ExternalAPI` 增加单一目标状态式设置接口和查询接口。
- `LinkageEngine` 注入 `ConfigManager`，执行和查询时读取开关，不再自行存储。
- `BackendBridge` 通过 `ExternalAPI` 操作开关。
- 前端暂存和批量应用流程不变。

详细设计见 `2026-07-28-event-action-switch-config-design.md`。

## 联动动作指定线程执行

### 用户需求

外部注册大量联动动作时，希望获得类似 Qt `connect()` 的线程亲和性：某些动作继续在线程池执行，另一些动作在调用方指定的 QObject 所属线程执行。

### 关键决策

- `registerAction()` 提供线程池与 QObject execution context 两个重载，均返回注册成功与否。
- context action 通过每 action 私有 dispatcher 和 `Qt::QueuedConnection` 调度；调用方维护 context 生命周期、线程和 event loop。
- action 名唯一且不可注销、不可替换；同名和非法注册拒绝，不修改已有 registration。
- context 使用 `QPointer` 非拥有引用；销毁后每次执行尝试安全跳过并尽可能记录日志，配置及显示名保留，同名仍不可重新注册。
- action 可用性与用户事件开关分离；有效 enabled 为两者合取。不可用项在前端保留，但取消勾选并禁用，不显示说明。
- callback 保持无参数、fire-and-forget；异常捕获记录，不重试、不传播。
- 不扩展 fallback，不提供生产清空接口；测试隔离使用测试专用 seam。

### 调用方契约

- context 在注册前完成线程设置，模块不管理或检查目标线程。
- callback 捕获非拥有引用、裸指针或 `QPointer`，不持有需特定线程析构的对象。
- callback 不释放捕获对象，且不得长时间阻塞其 context 线程。
- context 销毁属于调用方逻辑错误；模块只负责避免崩溃和暴露不可用状态。

详细设计见 `2026-07-28-linkage-action-execution-context-design.md`，架构决定见 `docs/adr/0001-context-bound-linkage-actions.md`。
