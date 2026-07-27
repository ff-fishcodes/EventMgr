# 事件联动开关配置归属与外部接口设计

## 1. 背景

当前联动动作开关由 `LinkageEngine` 的 `disabledActive_`、`disabledClear_` 保存，前端通过 `BackendBridge` 直接调用 `LinkageEngine::enableAction()` / `disableAction()`。这使事件配置分散在两个模块：降级和屏蔽由 `ConfigManager` 管理，联动开关由执行引擎管理；同时，业务代码无法通过 `ExternalAPI` 设置或查询联动开关。

2026-07-28，用户确认：联动开关属于事件配置，应由 `ConfigManager` 统一管理，并通过 `ExternalAPI` 暴露。当前仍只做进程内保存，不新增磁盘持久化。

## 2. 目标

- 将按事件、动作、阶段区分的联动开关移入 `ConfigManager`。
- 由 `ExternalAPI` 提供设置和查询接口。
- `LinkageEngine` 只负责联动定义、动作注册、解析和执行。
- 保持现有语义：未配置时默认启用；产生侧和消除侧独立；不同事件和动作互不影响。
- 前端暂存、批量应用和重新加载行为保持不变。

## 3. 非目标

- 不增加文件、数据库或其他持久化。
- 不改变事件专属联动和等级默认联动的定义接口。
- 不改变前端布局。
- 不增加批量事务或失败回滚。

## 4. 数据模型

`ConfigManager` 继续采用“仅保存例外”的最小模型：只保存关闭项。键由 EventId、动作名、阶段组成；产生侧和消除侧使用独立集合，避免分隔符拼接造成名称冲突。

```cpp
std::unordered_set<ActionSwitchKey, ActionSwitchKeyHash> disabledActiveActions_;
std::unordered_set<ActionSwitchKey, ActionSwitchKeyHash> disabledClearActions_;
```

若为减少新类型而继续使用字符串键，必须沿用现有 `eventId + "|" + actionName` 规则并明确名称约束。推荐使用结构化键，避免 EventId 或动作名包含 `|` 时冲突。

默认状态为启用；集合中存在对应键时为关闭。`ConfigManager::clearAll()` 同时清空两侧集合。

## 5. 接口

### 5.1 ConfigManager

```cpp
void setActionEnabled(const EventId& eventId,
                      const std::string& actionName,
                      bool isActive,
                      bool enabled);

bool isActionEnabled(const EventId& eventId,
                     const std::string& actionName,
                     bool isActive) const;
```

`setActionEnabled(..., false)` 插入禁用集合；`true` 删除禁用项。接口名称直接表达目标状态，替代成对的 enable/disable 方法。

### 5.2 ExternalAPI

```cpp
void setEventActionEnabled(const EventId& eventId,
                           const std::string& actionName,
                           bool isActive,
                           bool enabled);

bool isEventActionEnabled(const EventId& eventId,
                          const std::string& actionName,
                          bool isActive) const;
```

`ExternalAPI` 只转发到其已注入的 `ConfigManager`，保持业务代码不直接依赖内部模块。

### 5.3 LinkageEngine

构造函数改为注入 `ConfigManager&`。删除：

- `enableAction()`
- `disableAction()`
- `isActionDisabled()`
- `isActionDisabledLocked()`
- `disabledActive_`
- `disabledClear_`
- `makeDisableKey()`

执行动作和生成 `ActionInfo` 时调用 `ConfigManager::isActionEnabled()`。`LinkageEngine::clearAll()` 不再清理事件配置开关；测试或重启需要同时调用 `ConfigManager::clearAll()`。

## 6. 数据流

```text
AlarmCatalogWidget 暂存开关
  -> BackendBridge::setEventActionEnabled
  -> ExternalAPI::setEventActionEnabled
  -> ConfigManager::setActionEnabled

LinkageEngine 执行/查询
  -> ConfigManager::isActionEnabled
  -> 关闭则跳过执行；查询 DTO 的 enabled=false
```

`BackendBridge` 不再直接访问 `LinkageEngine` 修改开关。联动分组查询仍由 `LinkageEngine` 完成，因为动作合并、排序、显示名和阶段定义属于联动引擎。

## 7. 并发与错误处理

- `ConfigManager::mutex_` 保护降级、屏蔽和联动开关配置。
- `LinkageEngine` 不在持有 `configMutex_` 时获取 `ConfigManager` 锁后再回调自身，避免锁环。
- 当前两个模块没有反向调用；锁顺序不形成环。
- 设置未知事件或未知动作仍允许保存开关，与现有行为一致；执行和查询只有动作实际进入有效列表时才读取状态。
- 所有接口保持同步、内存操作，不增加错误返回。

## 8. 涉及文件

- `backend/config_manager.h/.cpp`：新增开关存储和设置/查询。
- `backend/external_api.h/.cpp`：新增公开设置/查询接口。
- `backend/linkage_engine.h/.cpp`：注入 ConfigManager，删除内部开关状态和公开开关 API。
- `backend/event_mgr_module.cpp`：构造 LinkageEngine 时注入 ConfigManager。
- `frontend/backend_bridge.h/.cpp`：统一成一个按目标状态设置的桥接方法，并只访问 ExternalAPI。
- `frontend/alarm_catalog_widget.h/.cpp`：差异应用改调单一设置接口，暂存结构不变。
- `tests/test_linkage_engine.cpp`、`tests/test_alarm_catalog_widget.cpp`：适配构造和入口，覆盖配置归属及执行一致性。

## 9. 测试

- ConfigManager 默认返回启用。
- 产生侧和消除侧状态独立。
- 不同 EventId、动作名状态独立。
- 关闭后查询 DTO 显示 disabled，执行跳过回调；重新开启后恢复。
- ExternalAPI 设置和查询与 ConfigManager 一致。
- `ConfigManager::clearAll()` 恢复默认启用。
- LinkageEngine 并发配置查询和执行测试继续通过。
- 报警配置页应用、放弃、取消和重新加载测试继续通过。
- 完整前端 qmake/make 构建通过。

## 10. 验收标准

1. 业务代码只通过 ExternalAPI 设置和查询事件联动开关。
2. BackendBridge 不直接调用 LinkageEngine 的开关修改方法。
3. LinkageEngine 不再持有开关配置集合。
4. 开关查询结果和实际执行行为一致。
5. 现有 UI 行为不变，自动化测试和构建通过。
