# Event Configuration Linkage Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在“报警配置”页为目录内每个事件统一提供降级、屏蔽以及产生侧/消除侧联动开关，并让界面查询、运行时执行和并发配置遵循同一套有效联动规则。

**Architecture:** `LinkageEngine` 负责等级默认与事件专属动作的分阶段合并、默认优先去重、禁用状态和线程安全快照；`BackendBridge` 只转换 Qt DTO；`AlarmCatalogWidget` 使用左侧目录表和右侧联动详情面板暂存全部修改，并在“应用配置”时提交差异。后端和前端分别建立 Qt Test 可执行目标，先用失败测试锁定契约，再做最小实现，最后同步当前需求、概要设计、详细设计、图表和讨论记录。

**Tech Stack:** C++11、Qt 5.15.10 目标版本（本机 Qt 5.15.3）、Qt5Core、Qt5Gui、Qt5Widgets、Qt5Test、Qt5Concurrent、qmake、make、GCC 11.4.0。

---

## File Map

| Path | Responsibility |
|---|---|
| `backend/linkage_engine.h` | 定义分阶段动作 DTO、双阶段等级默认配置、查询接口和统一配置锁 |
| `backend/linkage_engine.cpp` | 实现显式构造/析构、有效动作解析、锁内快照、锁外任务与 fallback 调用 |
| `backend/action_registry.cpp` | 把示例等级默认配置迁移到产生/消除双列表接口 |
| `frontend/backend_bridge.h/.cpp` | 将后端分阶段 DTO 转换为 Qt DTO，移除旧合并式查询 |
| `frontend/alarm_catalog_widget.ui` | 定义目录/联动固定分栏、产生侧和消除侧表格、底部操作区 |
| `frontend/alarm_catalog_widget.h/.cpp` | 维护跨行待提交状态、详情切换、脏数据确认、差异提交和状态反馈 |
| `frontend/frontend.pro` | 保持产品构建源文件完整，不引入新的第三方库 |
| `tests/linkage_engine_test.pro` | 后端 Qt Test 独立构建目标 |
| `tests/test_linkage_engine.cpp` | 双阶段默认、排序去重、阶段隔离、Info、fallback 和并发测试 |
| `tests/alarm_catalog_widget_test.pro` | 前端 Qt Test 独立构建目标 |
| `tests/test_alarm_catalog_widget.cpp` | 分栏、分组、暂存、应用和刷新交互测试 |
| `.gitignore` | 忽略 `.superpowers/` 视觉讨论缓存和新增测试构建目录 |
| `README.md`, `docs/README.md` | 更新使用说明、构建命令和当前文档导航 |
| `docs/superpowers/specs/2026-07-21-software-requirements-baseline.md` | 增加已实现需求和验收条目 |
| `docs/superpowers/specs/2026-07-21-high-level-design.md` | 更新模块职责、数据流、线程模型和第三方版本 |
| `docs/superpowers/specs/2026-07-21-detailed-design.md` | 更新类型、接口、算法、并发和前端状态机 |
| `docs/superpowers/specs/2026-07-21-architecture-diagrams.md` | 更新类图、调用关系图和产生/消除配置时序图 |
| `docs/superpowers/specs/2026-07-21-documentation-discussion-record.md` | 记录已确认需求、方案选择、测试证据和剩余风险 |

### Task 1: Establish Backend Test Harness And Failing Contract Tests

**Files:**
- Create: `tests/linkage_engine_test.pro`
- Create: `tests/test_linkage_engine.cpp`
- Modify: `.gitignore`

- [ ] **Step 1: Add deterministic backend test project**

Create `tests/linkage_engine_test.pro` with an isolated executable and only the production files required by `LinkageEngine`:

```qmake
QT += core testlib
TEMPLATE = app
CONFIG += console c++11 testcase
TARGET = test_linkage_engine

SOURCES += \
    test_linkage_engine.cpp \
    ../backend/linkage_engine.cpp

HEADERS += \
    ../backend/linkage_engine.h \
    ../backend/event_types.h

INCLUDEPATH += .. ../backend
```

Append these exact ignores to `.gitignore`, retaining all existing entries:

```gitignore
/.superpowers/
/build-tests/
```

- [ ] **Step 2: Write failing DTO, two-phase default, ordering and phase-isolation tests**

Create a Qt Test class whose `init()` and `cleanup()` call `engine_.clearAll()`. Define helpers `event(id, level)`, `names(actions)`, and `waitForCount(counter, expected)` in the test file. Add these concrete cases:

```cpp
class LinkageEngineTest : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void returnsDefaultsBeforeEventActionsAndDeduplicates();
    void keepsActiveAndClearActionsIndependent();
    void isolatesDisableStateByEventActionAndPhase();
    void suppressesInfoActionsAndFallback();
    void invokesFallbackWithoutHoldingConfigurationLock();
    void supportsConcurrentConfigurationQueryAndExecution();
private:
    LinkageEngine engine_;
};
```

In `returnsDefaultsBeforeEventActionsAndDeduplicates()`, register `default_active`, `shared`, `event_active`, `default_clear`, and `event_clear`; configure Emergency defaults as active `{default_active, shared}` and clear `{default_clear, shared}`; configure event `E-1-A` as active `{shared, event_active}` and clear `{shared, event_clear}`. Assert exact grouped names:

```cpp
QCOMPARE(names(groups.activeActions),
         QStringList() << "default_active" << "shared" << "event_active");
QCOMPARE(names(groups.clearActions),
         QStringList() << "default_clear" << "shared" << "event_clear");
QVERIFY(groups.activeActions.at(0).enabled);
QCOMPARE(QString::fromStdString(groups.activeActions.at(0).displayName),
         QString("默认产生"));
```

In `keepsActiveAndClearActionsIndependent()`, configure an active-only action and a clear-only action, then assert neither appears in the opposite list. In `isolatesDisableStateByEventActionAndPhase()`, disable `shared` only for `E-1-A` active and assert `E-1-A` clear plus both phases of `E-2-A` remain enabled.

- [ ] **Step 3: Write failing execution and thread-safety tests**

Use `QAtomicInt` counters and `QSemaphore`/`QTRY_COMPARE` so no sleep-based assertion is needed. Verify:

```cpp
engine_.setFallback([&](const std::string&, bool) {
    engine_.registerAction("from_fallback", "回调内注册", []() {});
    fallbackCount.ref();
});
engine_.executeActive(event("E-1-A", EventLevel::Emergency));
QCOMPARE(fallbackCount.loadAcquire(), 1);
```

This test times out or deadlocks if fallback runs while `configMutex_` is held. For concurrent pressure, run one writer loop that alternates `configureEvent`, `setLevelDefault`, `disableAction`, and `enableAction`, plus reader loops calling `getEventActionGroups`, `executeActive`, and `executeCleared`; join all threads and assert every returned group has no duplicate internal names. Keep the loop bounded at 500 iterations per worker so the test completes promptly under CI.

- [ ] **Step 4: Run the new tests and verify contract compilation fails**

Run:

```bash
mkdir -p build-tests/linkage
cd build-tests/linkage
qmake ../../tests/linkage_engine_test.pro
make -j4
```

Expected: compilation fails because `EventActionGroups`, the three-argument `setLevelDefault()`, and `getEventActionGroups()` do not exist yet. A link or runtime failure is not the intended red state.

- [ ] **Step 5: Commit the red backend test contract**

```bash
git add .gitignore tests/linkage_engine_test.pro tests/test_linkage_engine.cpp
git commit -m "test: define phased linkage configuration contract"
```

### Task 2: Implement Phased Effective Action Resolution

**Files:**
- Modify: `backend/linkage_engine.h`
- Modify: `backend/linkage_engine.cpp`
- Test: `tests/test_linkage_engine.cpp`

- [ ] **Step 1: Define explicitly constructed DTOs and level configuration**

Replace the old four-field `ActionInfo` and single-list level default with these declarations. Keep `LevelActionConfig` private and declare all touched constructors/destructors explicitly:

```cpp
struct ActionInfo {
    std::string name;
    std::string displayName;
    bool enabled;

    ActionInfo();
    ActionInfo(const std::string& actionName,
               const std::string& actionDisplayName,
               bool actionEnabled);
    ~ActionInfo();
};

struct EventActionGroups {
    std::vector<ActionInfo> activeActions;
    std::vector<ActionInfo> clearActions;

    EventActionGroups();
    ~EventActionGroups();
};
```

```cpp
struct LevelActionConfig {
    std::vector<std::string> activeActions;
    std::vector<std::string> clearActions;

    LevelActionConfig();
    LevelActionConfig(const std::vector<std::string>& active,
                      const std::vector<std::string>& clear);
    ~LevelActionConfig();
};
```

Change the public API to:

```cpp
void setLevelDefault(EventLevel level,
                     const std::vector<std::string>& activeActions,
                     const std::vector<std::string>& clearActions);
EventActionGroups getEventActionGroups(const EventId& eventId,
                                       EventLevel originalLevel) const;
```

Remove `getEventActions()`. Change `levelDefaults_` to `std::unordered_map<int, LevelActionConfig>`.

- [ ] **Step 2: Add one configuration mutex and non-locking helpers**

Include `QMutex`, add `mutable QMutex configMutex_;`, and declare helpers whose names make their lock precondition explicit:

```cpp
std::vector<std::string> resolveNamesLocked(
    const Event& event, bool isActive) const;
std::vector<std::string> effectiveConfiguredNamesLocked(
    const EventId& eventId, EventLevel originalLevel, bool isActive) const;
std::vector<ActionInfo> actionInfosLocked(
    const EventId& eventId,
    const std::vector<std::string>& names,
    bool isActive) const;
bool isActionDisabledLocked(const EventId& eventId,
                            const std::string& name,
                            bool isActive) const;
```

The comments must state that `configMutex_` protects `actionTable_`, `displayNames_`, `eventConfig_`, `levelDefaults_`, `fallback_`, and both disabled sets; callers of `*Locked` helpers must already hold it.

- [ ] **Step 3: Implement default-first stable deduplication**

Implement one private append helper in the `.cpp` anonymous namespace:

```cpp
void appendUnique(const std::vector<std::string>& source,
                  std::unordered_set<std::string>& seen,
                  std::vector<std::string>& target) {
    for (std::vector<std::string>::const_iterator it = source.begin();
         it != source.end(); ++it) {
        if (seen.insert(*it).second) target.push_back(*it);
    }
}
```

Both runtime resolution and catalog query must append the matching phase of `levelDefaults_` first, then the matching phase of `eventConfig_`. Runtime resolution alone falls back to `event.activeActions` or `event.clearActions` when no explicit event config exists. Return empty for `EventLevel::Info` before reading lists.

- [ ] **Step 4: Implement grouped query and display fallback**

Under one `QMutexLocker`, derive active and clear names and transform each name as:

```cpp
const std::unordered_map<std::string, std::string>::const_iterator displayIt =
    displayNames_.find(name);
const std::string displayName =
    displayIt == displayNames_.end() ? name : displayIt->second;
result.push_back(ActionInfo(
    name, displayName, !isActionDisabledLocked(eventId, name, isActive)));
```

The query must include configured names without registered callbacks; execution continues to skip them.

- [ ] **Step 5: Run backend tests and verify functional cases pass**

Run:

```bash
cd build-tests/linkage
qmake ../../tests/linkage_engine_test.pro
make -j4
./test_linkage_engine returnsDefaultsBeforeEventActionsAndDeduplicates \
    keepsActiveAndClearActionsIndependent \
    isolatesDisableStateByEventActionAndPhase
```

Expected: all selected cases report `PASS`; concurrency and fallback cases may remain red until Task 3.

- [ ] **Step 6: Commit phased resolution**

```bash
git add backend/linkage_engine.h backend/linkage_engine.cpp tests/test_linkage_engine.cpp
git commit -m "feat: resolve phased event linkages"
```

### Task 3: Protect Linkage Configuration And Execute Callbacks Outside Locks

**Files:**
- Modify: `backend/linkage_engine.cpp`
- Test: `tests/test_linkage_engine.cpp`

- [ ] **Step 1: Lock every configuration read and write**

Wrap `registerAction`, `setFallback`, `configureEvent`, `setLevelDefault`, `disableAction`, `enableAction`, `isActionDisabled`, and `getEventActionGroups` with `QMutexLocker locker(&configMutex_);`. Public `isActionDisabled()` delegates to `isActionDisabledLocked()` so no method recursively locks the non-recursive mutex.

- [ ] **Step 2: Snapshot callbacks before submitting work**

Change execution to copy callable objects under the lock and start tasks after unlock:

```cpp
std::vector<ActionCallback> callbacks;
{
    QMutexLocker locker(&configMutex_);
    for (std::vector<std::string>::const_iterator it = names.begin();
         it != names.end(); ++it) {
        if (isActionDisabledLocked(eventId, *it, isActive)) continue;
        const std::unordered_map<std::string, ActionCallback>::const_iterator found =
            actionTable_.find(*it);
        if (found != actionTable_.end()) callbacks.push_back(found->second);
    }
}
for (std::vector<ActionCallback>::const_iterator it = callbacks.begin();
     it != callbacks.end(); ++it) {
    linkagePool_.start(new ActionTask(*it));
}
```

Add `~ActionTask()` explicitly and mark `run()` with `override` if supported by the current Qt/C++11 declarations.

- [ ] **Step 3: Snapshot fallback and invoke it unlocked**

For each non-Info execution, resolve names and copy `fallback_` under one configuration lock, then release the lock before `executeNames()` and before invoking fallback. The implementation must not call a public locking method while already holding `configMutex_`.

```cpp
FallbackCallback fallback;
std::vector<std::string> names;
{
    QMutexLocker locker(&configMutex_);
    names = resolveNamesLocked(event, true);
    fallback = fallback_;
}
executeNames(names, event.id, true);
if (fallback) fallback(event.id, true);
```

Use the corresponding `false` phase for cleared events.

- [ ] **Step 4: Preserve the documented clearAll shutdown boundary**

Keep `linkagePool_.waitForDone()` before taking `configMutex_`; then lock and clear all protected containers, including `fallback_`:

```cpp
linkagePool_.waitForDone();
QMutexLocker locker(&configMutex_);
actionTable_.clear();
displayNames_.clear();
eventConfig_.clear();
levelDefaults_.clear();
fallback_ = FallbackCallback();
disabledActive_.clear();
disabledClear_.clear();
```

Comment that callers must stop new execution before `clearAll()` because this is a shutdown/test helper, not a full shutdown protocol.

- [ ] **Step 5: Run all backend tests, including concurrency pressure**

Run:

```bash
cd build-tests/linkage
make -j4
./test_linkage_engine -v1
```

Expected: all test slots pass, no test exceeds the Qt Test timeout, and the fallback re-entry case returns normally.

- [ ] **Step 6: Commit thread protection**

```bash
git add backend/linkage_engine.cpp tests/test_linkage_engine.cpp
git commit -m "fix: protect linkage configuration snapshots"
```

### Task 4: Migrate Registry And Qt Bridge APIs

**Files:**
- Modify: `backend/action_registry.cpp`
- Modify: `frontend/backend_bridge.h`
- Modify: `frontend/backend_bridge.cpp`
- Test: `tests/test_linkage_engine.cpp`

- [ ] **Step 1: Migrate every level-default caller**

Find all call sites:

```bash
rg -n "setLevelDefault|getEventActions" --glob '!docs/**' --glob '!build*/**'
```

Convert active-only defaults to an explicit empty clear side:

```cpp
engine.setLevelDefault(EventLevel::Emergency,
                       {"cooler_stop"},
                       std::vector<std::string>());
```

当前源码检索确认只有 `backend/action_registry.cpp` 调用该设置接口。迁移后不得保留两参数 `setLevelDefault()`，生产代码也不得保留 `getEventActions()`。

- [ ] **Step 2: Define explicitly constructed Qt grouped DTOs**

Replace `BackendBridge::ActionEntry` with:

```cpp
struct ActionEntry {
    QString name;
    QString displayName;
    bool enabled;

    ActionEntry();
    ActionEntry(const QString& actionName,
                const QString& actionDisplayName,
                bool actionEnabled);
    ~ActionEntry();
};

struct EventActionGroups {
    QVector<ActionEntry> activeActions;
    QVector<ActionEntry> clearActions;

    EventActionGroups();
    ~EventActionGroups();
};

EventActionGroups getEventActionGroups(const QString& eventId,
                                       int originalLevel) const;
```

Implement all constructors/destructors in `backend_bridge.cpp`, initializing `enabled` to `false` in the default constructor.

- [ ] **Step 3: Convert backend groups without business-rule duplication**

Call `LinkageEngine::getEventActionGroups(eventId.toStdString(), static_cast<EventLevel>(originalLevel))` once. Convert each backend vector in order to its matching Qt vector and copy `enabled` directly. Do not sort, merge, deduplicate, or infer phase in the bridge.

- [ ] **Step 4: Verify complete product compilation**

Run:

```bash
mkdir -p build-backend-plan
g++ -std=c++11 -fPIC -no-pie -pthread \
    -I backend -I backend/stubs \
    main.cpp backend/*.cpp backend/stubs/*.cpp \
    $(pkg-config --cflags --libs Qt5Core) \
    -o build-backend-plan/eventmgr-demo

mkdir -p build-frontend-plan
cd build-frontend-plan
qmake ../frontend/frontend.pro
make -j4
```

Expected: both `eventmgr-demo` and `EventMgrFrontend` build successfully with no obsolete API errors.

- [ ] **Step 5: Commit API migration**

```bash
git add backend/action_registry.cpp \
    frontend/backend_bridge.h frontend/backend_bridge.cpp
git commit -m "refactor: expose phased linkage groups to Qt"
```

### Task 5: Establish Frontend Test Harness And Failing UI Tests

**Files:**
- Create: `tests/alarm_catalog_widget_test.pro`
- Create: `tests/test_alarm_catalog_widget.cpp`
- Modify: `.gitignore`

- [ ] **Step 1: Add an offscreen-capable widget test project**

Create `tests/alarm_catalog_widget_test.pro` with this complete product dependency list:

```qmake
QT += core gui widgets testlib concurrent
TEMPLATE = app
CONFIG += console c++11 testcase
TARGET = test_alarm_catalog_widget

FORMS += \
    ../frontend/alarm_catalog_widget.ui \
    ../frontend/event_list_widget.ui

SOURCES += \
    test_alarm_catalog_widget.cpp \
    ../frontend/eventmgr_widget.cpp \
    ../frontend/alarm_catalog_widget.cpp \
    ../frontend/event_list_widget.cpp \
    ../frontend/backend_bridge.cpp \
    ../backend/external_api.cpp \
    ../backend/event_manager.cpp \
    ../backend/linkage_engine.cpp \
    ../backend/config_manager.cpp \
    ../backend/action_registry.cpp \
    ../backend/event_mgr_module.cpp \
    ../backend/system_events.cpp \
    ../backend/stubs/alarm_catalog.cpp \
    ../backend/stubs/socket_server.cpp \
    ../backend/stubs/log_writer.cpp \
    ../backend/stubs/cmd_sender.cpp \
    ../backend/stubs/buzzer_control.cpp

HEADERS += \
    ../frontend/eventmgr_widget.h \
    ../frontend/alarm_catalog_widget.h \
    ../frontend/event_list_widget.h \
    ../frontend/backend_bridge.h

INCLUDEPATH += .. ../backend ../frontend
```

- [ ] **Step 2: Write failing layout and grouped-list tests**

Use `QTEST_MAIN(AlarmCatalogWidgetTest)` and initialize one `BackendBridge` for the suite. In each test setup, call `LinkageEngine::instance().clearAll()` followed by `ActionRegistry::setup(LinkageEngine::instance())`, then clear any downgrade/shield changes made by the preceding case. This keeps the process-global backend deterministic. Find controls by stable object names and test these exact expectations:

```cpp
QVERIFY(widget.findChild<QSplitter*>("configSplitter"));
QCOMPARE(widget.findChild<QTableWidget*>("catalogTable")->columnCount(), 6);
QVERIFY(widget.findChild<QTableWidget*>("activeActionTable"));
QVERIFY(widget.findChild<QTableWidget*>("clearActionTable"));
```

Select `锅炉-3-temp_high`; assert the selected event ID is shown, the produced-action table contains `关冷却塔` before `调风扇`, the clear table shows `无联动`, and the catalog linkage count equals the number of phase entries.

- [ ] **Step 3: Write failing staged-edit and apply tests**

Toggle downgrade, shield, and one active linkage for the boiler row; select another row and return; assert all three staged values are preserved. Click `applyBtn`; then assert backend catalog state and `getEventActionGroups()` match the selected switches, `applyBtn` becomes disabled, and `statusLabel` is exactly `配置已应用`.

- [ ] **Step 4: Write failing dirty-refresh decision tests**

Expose the confirmation branch through a protected virtual method rather than automating a native modal:

```cpp
enum class DirtyDecision { Apply, Discard, Cancel };
virtual DirtyDecision confirmDirtyChanges();
```

Create a test subclass that returns a queued decision. Verify Apply writes pending diffs then reloads, Discard drops pending diffs and reloads backend state, and Cancel keeps both the visible staged state and current selection unchanged.

- [ ] **Step 5: Run frontend tests and verify the UI contract is red**

Run:

```bash
mkdir -p build-tests/catalog
cd build-tests/catalog
qmake ../../tests/alarm_catalog_widget_test.pro
make -j4
QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget -v1
```

Expected: compilation fails because the pending-state type and dirty-decision hook do not exist, or the runtime assertions fail because the splitter and linkage tables do not exist.

- [ ] **Step 6: Commit the red frontend test contract**

```bash
git add tests/alarm_catalog_widget_test.pro tests/test_alarm_catalog_widget.cpp .gitignore
git commit -m "test: define event configuration center interactions"
```

### Task 6: Build The Split Event Configuration Interface

**Files:**
- Modify: `frontend/alarm_catalog_widget.ui`
- Modify: `frontend/alarm_catalog_widget.h`
- Modify: `frontend/alarm_catalog_widget.cpp`
- Test: `tests/test_alarm_catalog_widget.cpp`

- [ ] **Step 1: Replace the single table area with a stable splitter layout**

In the `.ui`, retain the title, bottom buttons, and status label. Replace the direct table item with horizontal `QSplitter configSplitter`:

```text
configSplitter
  catalogPane
    catalogTable (6 columns: 事件编号, 报警描述, 原始等级, 降级, 屏蔽, 联动)
  linkagePane
    selectedEventLabel
    activeTitleLabel (事件产生时)
    activeActionTable (联动动作, 启用)
    clearTitleLabel (事件消除时)
    clearActionTable (联动动作, 启用)
```

Give the splitter a default ratio near 62:38, keep both panes resizable, use square checkboxes, and set table row selection/no editing. The narrow layout must remain usable through splitter resizing and table horizontal scrolling; do not hide either phase.

- [ ] **Step 2: Define explicit pending-state and widget lifecycle types**

Add an explicit `~AlarmCatalogWidget()` and this private value type:

```cpp
struct PendingEventConfig {
    bool originalDowngraded;
    bool downgraded;
    bool originalShielded;
    bool shielded;
    QMap<QString, bool> originalActiveActions;
    QMap<QString, bool> activeActions;
    QMap<QString, bool> originalClearActions;
    QMap<QString, bool> clearActions;

    PendingEventConfig();
    ~PendingEventConfig();
    bool isDirty() const;
};
```

Store `QMap<QString, PendingEventConfig> pendingByEvent_`, `QVector<BackendBridge::CatalogEntry> catalog_`, `QString selectedEventId_`, and `bool loadingUi_`. Initialize every scalar in constructors.

- [ ] **Step 3: Load backend snapshots and preserve staged values across row switches**

On catalog reload, query every catalog entry and its grouped actions once, initialize missing `PendingEventConfig` entries from backend values, and rebuild rows. On selection change, first persist visible action checkbox values to the prior event, then render the selected event from `pendingByEvent_`. Use `QSignalBlocker` or `loadingUi_` to prevent programmatic checkbox setup from marking data dirty.

The linkage-count cell must show:

```cpp
QString::number(config.activeActions.size() + config.clearActions.size())
```

The right tables must preserve backend order. For an empty phase, use one disabled row with text `无联动` and no checkbox.

- [ ] **Step 4: Track all edits and expose one dirty state**

Connect every row-level downgrade/shield checkbox and every action checkbox to update `pendingByEvent_`. `hasDirtyChanges()` returns true when any `PendingEventConfig::isDirty()` is true. Enable `applyBtn` only while dirty. Status text should distinguish normal summary, `有未应用配置`, and `配置已应用`.

- [ ] **Step 5: Run layout and staging tests**

Run:

```bash
cd build-tests/catalog
qmake ../../tests/alarm_catalog_widget_test.pro
make -j4
QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget \
    showsSplitCatalogAndPhasedActions preservesEditsAcrossSelectionChanges
```

Expected: both selected tests pass; apply/refresh cases may remain red until Task 7.

- [ ] **Step 6: Commit the configuration interface**

```bash
git add frontend/alarm_catalog_widget.ui \
    frontend/alarm_catalog_widget.h frontend/alarm_catalog_widget.cpp \
    tests/test_alarm_catalog_widget.cpp
git commit -m "feat: add phased linkage controls to event catalog"
```

### Task 7: Apply Diffs And Guard Destructive Refreshes

**Files:**
- Modify: `frontend/alarm_catalog_widget.h`
- Modify: `frontend/alarm_catalog_widget.cpp`
- Modify: `frontend/eventmgr_widget.h`
- Modify: `frontend/eventmgr_widget.cpp`
- Test: `tests/test_alarm_catalog_widget.cpp`

- [ ] **Step 1: Submit only changed downgrade and shield values**

For each dirty event, compare staged values with originals. Call `setDowngrade(id, 4)` or `removeDowngrade(id)` only when downgrade differs; call `setShield(id)` or `clearShield(id)` only when shield differs.

- [ ] **Step 2: Submit action diffs with the correct phase**

Use one helper that compares original and staged maps:

```cpp
void AlarmCatalogWidget::applyActionDiffs(
        const QString& eventId,
        const QMap<QString, bool>& original,
        const QMap<QString, bool>& current,
        bool isActive) {
    for (QMap<QString, bool>::const_iterator it = current.begin();
         it != current.end(); ++it) {
        if (original.value(it.key()) == it.value()) continue;
        if (it.value()) bridge_->enableAction(eventId, it.key(), isActive);
        else bridge_->disableAction(eventId, it.key(), isActive);
    }
}
```

Invoke it once with `true` for produced actions and once with `false` for cleared actions. After all writes, reload from backend, restore the selected event when it still exists, disable Apply, and set status to `配置已应用`.

- [ ] **Step 3: Implement Apply/Discard/Cancel refresh semantics**

Before refresh, if clean, reload immediately. If dirty, `confirmDirtyChanges()` shows a `QMessageBox` with `应用`, `放弃`, and `取消`. Apply calls the same commit path then refreshes; Discard clears pending state then reloads; Cancel returns without changing selection, tables, or status.

Use the same guard from a public `requestReload()` slot and connect `refreshBtn` to it instead of directly to `loadCatalog()`. Keep `loadCatalog()` as the internal backend reload routine so tests can distinguish guarded and unguarded behavior.

- [ ] **Step 4: Guard leaving the alarm configuration tab**

Expose `bool requestLeave()` from `AlarmCatalogWidget`; it returns `true` for clean, Apply, or Discard, and `false` for Cancel. Add `int previousTabIndex_` to `EventMgrWidget`, initialize it to `0`, and in `onTabChanged(int index)` check `catalogPage_->requestLeave()` when `previousTabIndex_ == 1 && index != 1`. On `false`, restore tab index `1` under `QSignalBlocker`; on `true`, update `previousTabIndex_` and refresh the destination page. Apply/Discard/Cancel semantics must match refresh exactly.

- [ ] **Step 5: Run all frontend tests**

Run:

```bash
cd build-tests/catalog
make -j4
QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget -v1
```

Expected: all layout, phase, staging, apply, refresh, and tab-leave tests pass.

- [ ] **Step 6: Commit staged apply behavior**

```bash
git add frontend/alarm_catalog_widget.h frontend/alarm_catalog_widget.cpp \
    frontend/eventmgr_widget.h frontend/eventmgr_widget.cpp \
    tests/test_alarm_catalog_widget.cpp
git commit -m "feat: stage and apply event configuration changes"
```

### Task 8: Verify Product Build And Responsive Rendering

**Files:**
- Verify: `frontend/alarm_catalog_widget.ui`
- Verify: `frontend/alarm_catalog_widget.cpp`
- Modify: `tests/test_alarm_catalog_widget.cpp`

- [ ] **Step 1: Run both automated suites from clean plan build directories**

```bash
cd build-tests/linkage
qmake ../../tests/linkage_engine_test.pro
make clean
make -j4
./test_linkage_engine -v1

cd ../catalog
qmake ../../tests/alarm_catalog_widget_test.pro
make clean
make -j4
QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget -v1
```

Expected: zero failed tests in both executables.

- [ ] **Step 2: Rebuild backend demo and complete frontend**

```bash
g++ -std=c++11 -fPIC -no-pie -pthread \
    -I backend -I backend/stubs \
    main.cpp backend/*.cpp backend/stubs/*.cpp \
    $(pkg-config --cflags --libs Qt5Core) \
    -o build-backend-plan/eventmgr-demo

cd build-frontend-plan
qmake ../frontend/frontend.pro
make clean
make -j4
```

Expected: both targets compile and link. Do not use the tracked, user-modified `build/test_eventmgr` as verification output.

- [ ] **Step 3: Capture deterministic offscreen widget screenshots**

Add a test helper that resizes the widget to `1280x760` and `760x720`, calls `show()`, waits for exposure processing, and saves `widget.grab()` to `build-tests/screenshots/catalog-desktop.png` and `catalog-narrow.png`. Assert `QImage::isNull()` is false, width/height match the requested sizes, and a sampled bounding rectangle has more than one color so a blank render fails.

Run:

```bash
mkdir -p build-tests/screenshots
cd build-tests/catalog
QT_QPA_PLATFORM=offscreen EVENTMGR_SCREENSHOT_DIR=../screenshots \
    ./test_alarm_catalog_widget capturesResponsiveScreenshots
```

Expected: both PNG files exist, are nonblank, and the test passes.

- [ ] **Step 4: Inspect both screenshots and correct only concrete defects**

Open both PNGs and verify: no overlap; both phase headings remain visible; long event IDs and action names do not cover switches; button labels fit; the next section is not relevant because this is a work surface; selected row, enabled state, and empty state are visually distinguishable. A failed assertion or observed defect is handled through `superpowers:systematic-debugging`; adjust the identified splitter minimum size, header resize mode, row height, or word wrapping and rerun Step 3.

- [ ] **Step 5: Commit verified visual corrections**

```bash
git add frontend/alarm_catalog_widget.ui frontend/alarm_catalog_widget.cpp \
    tests/test_alarm_catalog_widget.cpp
git commit -m "fix: stabilize event configuration layout"
```

If no source correction was necessary, skip this commit; generated screenshots remain ignored build evidence.

### Task 9: Update Requirements, Designs, Diagrams And Discussion Record

**Files:**
- Modify: `README.md`
- Modify: `docs/README.md`
- Modify: `docs/superpowers/specs/2026-07-21-software-requirements-baseline.md`
- Modify: `docs/superpowers/specs/2026-07-21-high-level-design.md`
- Modify: `docs/superpowers/specs/2026-07-21-detailed-design.md`
- Modify: `docs/superpowers/specs/2026-07-21-architecture-diagrams.md`
- Modify: `docs/superpowers/specs/2026-07-21-documentation-discussion-record.md`

- [ ] **Step 1: Update the current requirement baseline and traceability**

Add uniquely numbered requirements covering: all catalog events expose three controls; active/clear phase separation; default-first effective list; per-event/action/phase enablement; staged unified apply; dirty refresh/leave decisions; no persistence; Info no linkage; and thread-safe configuration snapshots. Add acceptance rows pointing to the two Qt Test executables and the screenshot check. Do not revise historical requirement documents as though they were current.

- [ ] **Step 2: Update high-level and detailed design to match code exactly**

In the high-level design, replace the “LinkageEngine containers are unlocked” state with the single-`QMutex` snapshot model and update the configuration data flow. In detailed design, replace old `ActionInfo`, `ActionEntry`, `getEventActions()`, and two-argument `setLevelDefault()` descriptions with the implemented types/signatures; document constructors/destructors, lock ownership, default-first stable dedupe, missing callback/display behavior, pending UI state, Apply/Discard/Cancel behavior, and `clearAll()` shutdown-only precondition.

- [ ] **Step 3: Update class, call, and sequence diagrams**

Update Mermaid diagrams with:

```text
AlarmCatalogWidget --> PendingEventConfig
AlarmCatalogWidget --> BackendBridge::EventActionGroups
BackendBridge --> LinkageEngine::EventActionGroups
LinkageEngine *-- LevelActionConfig
LinkageEngine *-- QMutex configMutex_
```

Add/replace call graphs for catalog selection/query and unified apply. Add separate sequence diagrams for produced-action execution and cleared-action execution, showing lock -> snapshot -> unlock -> thread-pool submit -> unlocked fallback. Add the dirty refresh/tab-leave Apply/Discard/Cancel sequence.

- [ ] **Step 4: Record this discussion and concrete verification evidence**

Append a dated record summarizing the user-confirmed choices: full catalog scope, separate produced/cleared lists, default actions first without source labels, three-argument default API, fixed right detail panel, staged unified apply, and one mutex protecting all linkage configuration. Record exact tool versions and the pass/fail totals from Task 8; retain unrelated known risks such as GUI event-notification re-entry deadlock and lack of persistence.

- [ ] **Step 5: Update README and document navigation**

Update the linkage example to the three-argument API; describe the new configuration page workflow without claiming transactional rollback or persistence. Add Qt5Test 5.15.10 to the target dependency/build table and record local Qt5Test 5.15.3 separately. Add exact test commands and link the approved design plus this implementation plan from `docs/README.md`.

- [ ] **Step 6: Run documentation consistency checks**

Run:

```bash
rg -n "setLevelDefault\([^,\n]+,[^,\n]+\)|getEventActions|disabledActive|disabledClear|配置容器没有锁|配置容器无锁" \
    README.md docs backend frontend tests
rg -n "EventActionGroups|getEventActionGroups|configMutex_|Qt5Test|产生侧|消除侧" \
    README.md docs/superpowers/specs/2026-07-21-*.md
```

Expected: the first command finds no stale current-code/current-baseline claims; any matches in explicitly historical sections are labeled historical. The second command finds the new interfaces, phase semantics, mutex, and test dependency in the current documents.

- [ ] **Step 7: Commit documentation before any remote push**

```bash
git add README.md docs/README.md \
    docs/superpowers/specs/2026-07-21-software-requirements-baseline.md \
    docs/superpowers/specs/2026-07-21-high-level-design.md \
    docs/superpowers/specs/2026-07-21-detailed-design.md \
    docs/superpowers/specs/2026-07-21-architecture-diagrams.md \
    docs/superpowers/specs/2026-07-21-documentation-discussion-record.md
git commit -m "docs: document event linkage configuration center"
```

### Task 10: Final Verification And Review Handoff

**Files:**
- Verify: all files from Tasks 1-9

- [ ] **Step 1: Run the complete verification matrix again**

```bash
cd build-tests/linkage && ./test_linkage_engine -v1
cd ../catalog && QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget -v1
cd ../../build-frontend-plan && make -j4
cd ../build-backend-plan && ./eventmgr-demo
```

Expected: both test executables report zero failures; both products are already built; the backend demo runs through its fixed scenario and exits normally. The demo may print asynchronous action output in nondeterministic order.

- [ ] **Step 2: Audit constructors, destructors, comments and obsolete interfaces**

```bash
rg -n "struct (ActionInfo|EventActionGroups|LevelActionConfig|PendingEventConfig)|class (ActionTask|AlarmCatalogWidget)" \
    backend frontend
rg -n "getEventActions|setLevelDefault" --glob '!docs/superpowers/specs/2026-06-*' \
    --glob '!docs/superpowers/specs/2026-07-06-*' .
git diff --check a008546..HEAD
```

Expected: every touched class/struct has explicit constructor and destructor declarations/definitions; comments explain lock preconditions, phase semantics, and shutdown boundary; no production use of the obsolete merged query or two-argument default API remains; `git diff --check` reports no whitespace errors. `a008546` 是本功能设计提交，因而该范围稳定覆盖计划及其后的全部实现提交。

- [ ] **Step 3: Inspect repository status without disturbing user files**

```bash
git status --short --branch
git log --oneline --decorate -10
```

Expected: implementation and documentation are committed; pre-existing user changes such as `build/test_eventmgr`, `frontend/frontend.pro.user`, `.vscode/c_cpp_properties.json`, and `build-frontend-unknown-Debug/` remain untouched and uncommitted. `.superpowers/` no longer appears because it is ignored.

- [ ] **Step 4: Request code review before integration**

Use `superpowers:requesting-code-review` against the full implementation commit range. Require findings to check phase correctness, lock recursion/deadlock risks, callback lifetime, staged UI state loss, exact docs/code consistency, and missing tests. Resolve all high/medium findings and rerun Steps 1-3.

- [ ] **Step 5: Present integration choices without pushing**

Use `superpowers:finishing-a-development-branch` only after review and verification are clean. Report the commit range and verification evidence. Do not push: the project rule requires documentation before push, and remote integration still requires the user's explicit choice.

## 2026-07-23 Task 8 screenshot quality follow-up

### Discussion and decision record

- The original screenshot assertion treated overflow as a fixed property of a
  viewport: desktop active and both clear tables were required to have no
  overflowing labels. `QT_FONT_DPI=120` and `QT_FONT_DPI=144` both disproved
  that assumption at `verifyActionTextRendering()` with 2 passed / 1 failed.
- The accepted contract is now per label and font-metric driven. Full text that
  fits must remain unchanged; overflowing text must have a nonempty fitting
  `displayText`, differ from the full value, carry explicit right ellipsis, and
  retain the full tooltip and accessible identity. Only the 760 px active table
  fixture must contain at least one overflow case. Desktop and clear tables may
  overflow when the selected font metrics require it.
- Duplicate same-phase actions are identified through the action cell property
  and the internal-name accessible role. At narrow width, their nonempty painted
  `displayText` values must differ while `accessibleName` remains each exact
  backend key. A targeted DPR-aware screenshot assertion also compares aligned
  foreground masks from the two mapped label `contentsRect` regions, so neither
  `QLabel::text()` nor the `displayText` property alone is accepted as
  visible-paint evidence.
- Screenshot evidence keeps broad table checks and adds DPR-aware luminance and
  edge sampling for mapped action-label `contentsRect` values and the separate
  refresh-button, apply-button, and status-text regions. The checks avoid exact
  palette colors. Under `QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1`, a
  temporary blank `ElidedLabel::paintEvent()` mutation triggered multiple
  targeted paint failures. A separate temporary literal-`X` paint mutation made
  the duplicate-region glyph-mask assertion fail. Production source was restored
  after both mutation checks.
- Commit `c18b6eb` retains the production `ElidedLabel` implementation and its
  responsive elision behavior. Commit `fc16c07` and this follow-up only harden
  tests and update this traceability record; they do not retain another
  production change, change a dependency, or push remotely.

### Verification evidence

Local verification used QtTest/Qt 5.15.3 and GCC 11.3.0 as reported by the test
executables. The project target remains Qt 5.15.10; this local evidence does not
replace target-platform qualification.

| Check | Result |
|---|---|
| Screenshot slot, `QT_SCALE_FACTOR=1` | 3 passed / 0 failed; regenerated 1280x760 and 760x720 PNG files and inspected both |
| Screenshot slot, `QT_SCALE_FACTOR=2` | 3 passed / 0 failed |
| Screenshot slot, `QT_FONT_DPI=120` | 3 passed / 0 failed |
| Screenshot slot, `QT_FONT_DPI=144` | 3 passed / 0 failed |
| Complete frontend Qt Test executable | 19 passed / 0 failed |
| Complete backend Qt Test executable | 12 passed / 0 failed |
| Frontend qmake product | Existing qmake build relinked successfully |
| Backend demo product | README `g++ -std=c++11` plus Qt5Core command rebuilt successfully |

Generated screenshots remain ignored build evidence. Task 9 must update the
current detailed design and architecture diagrams to include `ElidedLabel`, its
class relationship, and its width-dependent visual behavior; the existing
diagrams do not yet cover that retained `c18b6eb` production change.
