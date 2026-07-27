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
