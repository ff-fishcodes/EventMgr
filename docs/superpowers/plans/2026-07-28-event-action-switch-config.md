# Event Action Switch Configuration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move per-event, per-action, per-phase linkage switches from `LinkageEngine` into `ConfigManager` and expose them through `ExternalAPI`, while preserving current UI and execution behavior.

**Architecture:** `ConfigManager` becomes the sole owner of downgrade, shield, and linkage-switch state. `ExternalAPI` exposes switch mutation/query; `LinkageEngine` receives `ConfigManager&` and consults it while producing action DTOs and selecting callbacks. `BackendBridge` routes UI changes through `ExternalAPI`; frontend staging remains unchanged.

**Tech Stack:** C++11, Qt 5 Core/Widgets/Concurrent/Test, qmake, STL unordered containers.

---

## File Structure

- Modify `backend/config_manager.h/.cpp`: own structured action-switch keys and provide thread-safe set/query operations.
- Modify `backend/external_api.h/.cpp`: expose event-action switch API through the existing facade.
- Modify `backend/linkage_engine.h/.cpp`: inject `ConfigManager`, consume its switch state, remove duplicate storage and methods.
- Modify `backend/event_mgr_module.cpp`: wire the existing `ConfigManager` into `LinkageEngine`.
- Modify `frontend/backend_bridge.h/.cpp`: replace paired enable/disable methods with one target-state method routed through `ExternalAPI`.
- Modify `frontend/alarm_catalog_widget.h/.cpp`: submit staged action diffs through the single bridge method.
- Modify `tests/linkage_engine_test.pro` and `tests/test_linkage_engine.cpp`: link `ConfigManager`, test ownership and execution/query consistency.
- Modify `tests/test_alarm_catalog_widget.cpp`: replace direct engine switch access with `ExternalAPI`/`ConfigManager` assertions.
- Preserve the existing unstaged `backend/external_api.cpp` clear-event simplification as a separate change; do not include it in feature commits.

### Task 1: Add Linkage Switch State to ConfigManager

**Files:**
- Modify: `backend/config_manager.h`
- Modify: `backend/config_manager.cpp`
- Test: `tests/test_linkage_engine.cpp`
- Modify test project: `tests/linkage_engine_test.pro`

- [ ] **Step 1: Update the test fixture to construct LinkageEngine with ConfigManager**

In `tests/test_linkage_engine.cpp`, add a `ConfigManager` fixture while leaving the current default-constructed `LinkageEngine` unchanged until Task 2:

```cpp
private:
    ConfigManager config_;
    LinkageEngine engine_;
};

LinkageEngineTest::LinkageEngineTest(QObject* parent)
    : QObject(parent), config_(), engine_() {}

void LinkageEngineTest::init() {
    config_.clearAll();
    engine_.clearAll();
    engine_.setFallback(LinkageEngine::FallbackCallback());
}
```

Add `#include "../backend/config_manager.h"`.

In `tests/linkage_engine_test.pro`, add:

```qmake
SOURCES += \
    test_linkage_engine.cpp \
    ../backend/linkage_engine.cpp \
    ../backend/config_manager.cpp

HEADERS += \
    ../backend/linkage_engine.h \
    ../backend/config_manager.h \
    ../backend/event_types.h
```

- [ ] **Step 2: Add a failing ConfigManager switch test**

Add slot declaration:

```cpp
void storesActionSwitchesByEventActionAndPhase();
```

Add test:

```cpp
void LinkageEngineTest::storesActionSwitchesByEventActionAndPhase() {
    QVERIFY(config_.isActionEnabled("E-1-A", "shared", true));
    QVERIFY(config_.isActionEnabled("E-1-A", "shared", false));

    config_.setActionEnabled("E-1-A", "shared", true, false);
    config_.setActionEnabled("E-1-A", "other", false, false);

    QVERIFY(!config_.isActionEnabled("E-1-A", "shared", true));
    QVERIFY(config_.isActionEnabled("E-1-A", "shared", false));
    QVERIFY(config_.isActionEnabled("E-2-A", "shared", true));
    QVERIFY(config_.isActionEnabled("E-1-A", "other", true));
    QVERIFY(!config_.isActionEnabled("E-1-A", "other", false));

    config_.setActionEnabled("E-1-A", "shared", true, true);
    QVERIFY(config_.isActionEnabled("E-1-A", "shared", true));

    config_.clearAll();
    QVERIFY(config_.isActionEnabled("E-1-A", "other", false));
}
```

- [ ] **Step 3: Run the test build and verify RED**

Run from an external build directory:

```bash
rm -rf /tmp/eventmgr-linkage-test
mkdir /tmp/eventmgr-linkage-test
cd /tmp/eventmgr-linkage-test
qmake /home/ff/EventMgr/tests/linkage_engine_test.pro
make -j4
```

Expected: compile fails because `ConfigManager::setActionEnabled` and `isActionEnabled` do not exist.

- [ ] **Step 4: Implement structured ConfigManager keys**

In `backend/config_manager.h`, add `<string>` and define private key/hash types:

```cpp
struct ActionSwitchKey {
    EventId eventId;
    std::string actionName;

    ActionSwitchKey(const EventId& id, const std::string& name)
        : eventId(id), actionName(name) {}

    bool operator==(const ActionSwitchKey& other) const {
        return eventId == other.eventId && actionName == other.actionName;
    }
};

struct ActionSwitchKeyHash {
    size_t operator()(const ActionSwitchKey& key) const {
        const size_t h1 = std::hash<std::string>()(key.eventId);
        const size_t h2 = std::hash<std::string>()(key.actionName);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
```

Add public methods:

```cpp
void setActionEnabled(const EventId& eventId,
                      const std::string& actionName,
                      bool isActive,
                      bool enabled);
bool isActionEnabled(const EventId& eventId,
                     const std::string& actionName,
                     bool isActive) const;
```

Add private sets:

```cpp
std::unordered_set<ActionSwitchKey, ActionSwitchKeyHash> disabledActiveActions_;
std::unordered_set<ActionSwitchKey, ActionSwitchKeyHash> disabledClearActions_;
```

In `backend/config_manager.cpp`, implement:

```cpp
void ConfigManager::setActionEnabled(const EventId& eventId,
                                     const std::string& actionName,
                                     bool isActive,
                                     bool enabled) {
    QMutexLocker locker(&mutex_);
    std::unordered_set<ActionSwitchKey, ActionSwitchKeyHash>& disabled =
        isActive ? disabledActiveActions_ : disabledClearActions_;
    const ActionSwitchKey key(eventId, actionName);
    if (enabled) disabled.erase(key);
    else         disabled.insert(key);
}

bool ConfigManager::isActionEnabled(const EventId& eventId,
                                    const std::string& actionName,
                                    bool isActive) const {
    QMutexLocker locker(&mutex_);
    const std::unordered_set<ActionSwitchKey, ActionSwitchKeyHash>& disabled =
        isActive ? disabledActiveActions_ : disabledClearActions_;
    return disabled.find(ActionSwitchKey(eventId, actionName)) == disabled.end();
}
```

Extend `clearAll()`:

```cpp
disabledActiveActions_.clear();
disabledClearActions_.clear();
```

- [ ] **Step 5: Run the ConfigManager test and verify GREEN**

Run:

```bash
cd /tmp/eventmgr-linkage-test
qmake /home/ff/EventMgr/tests/linkage_engine_test.pro
make -j4
./test_linkage_engine -txt
```

Expected: all tests, including `storesActionSwitchesByEventActionAndPhase`, pass.

- [ ] **Step 6: Commit ConfigManager ownership**

```bash
git add backend/config_manager.h backend/config_manager.cpp \
        tests/linkage_engine_test.pro tests/test_linkage_engine.cpp
git commit -m "feat: ConfigManager 管理事件联动开关"
```

Do not stage `backend/external_api.cpp` yet; it contains the separate clear-event simplification.

### Task 2: Make LinkageEngine Consume ConfigManager

**Files:**
- Modify: `backend/linkage_engine.h`
- Modify: `backend/linkage_engine.cpp`
- Modify: `backend/event_mgr_module.cpp`
- Test: `tests/test_linkage_engine.cpp`

- [ ] **Step 1: Change existing isolation test to configure ConfigManager**

Replace switch mutations/assertions in `isolatesDisableStateByEventActionAndPhase()`:

```cpp
config_.setActionEnabled("E-1-A", "shared", true, false);
config_.setActionEnabled("E-1-A", "other", false, false);
```

Keep existing `getEventActionGroups()` enabled-state assertions. Replace re-enable with:

```cpp
config_.setActionEnabled("E-1-A", "shared", true, true);
```

- [ ] **Step 2: Add an execution test proving the shared config is honored**

Add slot:

```cpp
void skipsActionsDisabledThroughConfigManager();
```

Add test:

```cpp
void LinkageEngineTest::skipsActionsDisabledThroughConfigManager() {
    QAtomicInt invoked(0);
    engine_.registerAction("shared", "shared", [&invoked]() { invoked.ref(); });
    engine_.configureEvent("E-1-A", {"shared"}, {"shared"});

    Event event = makeEvent("E-1-A", EventLevel::Emergency);
    config_.setActionEnabled(event.id, "shared", true, false);

    engine_.executeActive(event);
    engine_.clearAll();
    QCOMPARE(invoked.loadAcquire(), 0);

    engine_.registerAction("shared", "shared", [&invoked]() { invoked.ref(); });
    engine_.configureEvent("E-1-A", {"shared"}, {"shared"});
    config_.setActionEnabled(event.id, "shared", true, true);
    engine_.executeActive(event);
    engine_.clearAll();
    QCOMPARE(invoked.loadAcquire(), 1);
}
```

Use the existing `makeEvent()` test helper; do not create a duplicate.

- [ ] **Step 3: Run tests and verify RED**

Run:

```bash
cd /tmp/eventmgr-linkage-test
qmake /home/ff/EventMgr/tests/linkage_engine_test.pro
make -j4
./test_linkage_engine -txt
```

Expected: enabled-state and execution assertions fail because `LinkageEngine` still reads its own disabled sets.

- [ ] **Step 4: Inject ConfigManager and remove duplicate switch API/storage**

In `backend/linkage_engine.h`:

```cpp
class ConfigManager;

explicit LinkageEngine(ConfigManager& configMgr);
```

Delete declarations for `disableAction`, `enableAction`, `isActionDisabled`, `isActionDisabledLocked`, `makeDisableKey`, `disabledActive_`, and `disabledClear_`. Add:

```cpp
ConfigManager& configMgr_;
```

In `backend/linkage_engine.cpp`, include `config_manager.h`, replace constructor:

```cpp
LinkageEngine::LinkageEngine(ConfigManager& configMgr)
    : configMgr_(configMgr) {
    linkagePool_.setMaxThreadCount(4);
}
```

Delete the old switch methods and `makeDisableKey()`.

In `executeNames()` replace:

```cpp
if (isActionDisabledLocked(eventId, *it, isActive)) continue;
```

with:

```cpp
if (!configMgr_.isActionEnabled(eventId, *it, isActive)) continue;
```

In `actionInfosLocked()` replace enabled calculation with:

```cpp
configMgr_.isActionEnabled(eventId, *it, isActive)
```

In `clearAll()`, remove clearing the two deleted sets.

In `backend/event_mgr_module.cpp`, construct:

```cpp
linkageEng_ = new LinkageEngine(*configMgr_);
```

In `tests/test_linkage_engine.cpp`, update the fixture constructor to inject its `ConfigManager`:

```cpp
LinkageEngineTest::LinkageEngineTest(QObject* parent)
    : QObject(parent), config_(), engine_(config_) {}
```

- [ ] **Step 5: Run LinkageEngine tests and verify GREEN**

Run:

```bash
cd /tmp/eventmgr-linkage-test
qmake /home/ff/EventMgr/tests/linkage_engine_test.pro
make -j4
./test_linkage_engine -txt
```

Expected: all tests pass, including switch isolation and execution.

- [ ] **Step 6: Commit LinkageEngine migration**

```bash
git add backend/linkage_engine.h backend/linkage_engine.cpp \
        backend/event_mgr_module.cpp tests/test_linkage_engine.cpp
git commit -m "refactor: LinkageEngine 从事件配置读取联动开关"
```

### Task 3: Expose Linkage Switches Through ExternalAPI

**Files:**
- Modify: `backend/external_api.h`
- Modify: `backend/external_api.cpp`
- Test: `tests/test_alarm_catalog_widget.cpp`

> `backend/external_api.cpp` already has an unstaged clear-event simplification. Use `git diff backend/external_api.cpp` before editing. At commit time, stage only this task's API hunks with `git add -p backend/external_api.cpp`; leave the clear-event hunk unstaged.

- [ ] **Step 1: Add a failing facade test**

Add a slot in `AlarmCatalogWidgetTest`:

```cpp
void externalApiOwnsEventActionSwitches();
```

Add test:

```cpp
void AlarmCatalogWidgetTest::externalApiOwnsEventActionSwitches() {
    ExternalAPI& api = ExternalAPI::instance();
    const std::string eventId = kBoilerEvent.toStdString();

    QVERIFY(api.isEventActionEnabled(eventId, "cooler_fan", true));
    api.setEventActionEnabled(eventId, "cooler_fan", true, false);
    QVERIFY(!api.isEventActionEnabled(eventId, "cooler_fan", true));
    QVERIFY(api.isEventActionEnabled(eventId, "cooler_fan", false));

    api.setEventActionEnabled(eventId, "cooler_fan", true, true);
    QVERIFY(api.isEventActionEnabled(eventId, "cooler_fan", true));
}
```

- [ ] **Step 2: Run the frontend test build and verify RED**

Run:

```bash
rm -rf /tmp/eventmgr-catalog-test
mkdir /tmp/eventmgr-catalog-test
cd /tmp/eventmgr-catalog-test
qmake /home/ff/EventMgr/tests/alarm_catalog_widget_test.pro
make -j4
```

Expected: compile fails because the two ExternalAPI methods do not exist.

- [ ] **Step 3: Add ExternalAPI methods**

In `backend/external_api.h`, under the event-configuration facade section, add:

```cpp
void setEventActionEnabled(const EventId& eventId,
                           const std::string& actionName,
                           bool isActive,
                           bool enabled);
bool isEventActionEnabled(const EventId& eventId,
                          const std::string& actionName,
                          bool isActive) const;
```

In `backend/external_api.cpp`, add:

```cpp
void ExternalAPI::setEventActionEnabled(const EventId& eventId,
                                        const std::string& actionName,
                                        bool isActive,
                                        bool enabled) {
    configMgr_.setActionEnabled(eventId, actionName, isActive, enabled);
}

bool ExternalAPI::isEventActionEnabled(const EventId& eventId,
                                       const std::string& actionName,
                                       bool isActive) const {
    return configMgr_.isActionEnabled(eventId, actionName, isActive);
}
```

- [ ] **Step 4: Run the facade test and verify GREEN**

Run:

```bash
cd /tmp/eventmgr-catalog-test
qmake /home/ff/EventMgr/tests/alarm_catalog_widget_test.pro
make -j4
QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget -txt externalApiOwnsEventActionSwitches
```

Expected: one test plus init/cleanup passes.

- [ ] **Step 5: Commit only ExternalAPI feature hunks**

```bash
git add backend/external_api.h tests/test_alarm_catalog_widget.cpp
git add -p backend/external_api.cpp
```

Accept only `setEventActionEnabled` / `isEventActionEnabled` hunks; reject the pre-existing `clearEvent` hunk. Verify:

```bash
git diff --cached -- backend/external_api.cpp
git diff -- backend/external_api.cpp
```

Then commit:

```bash
git commit -m "feat: ExternalAPI 提供事件联动开关接口"
```

### Task 4: Route BackendBridge and UI Through ExternalAPI

**Files:**
- Modify: `frontend/backend_bridge.h`
- Modify: `frontend/backend_bridge.cpp`
- Modify: `frontend/alarm_catalog_widget.h`
- Modify: `frontend/alarm_catalog_widget.cpp`
- Test: `tests/test_alarm_catalog_widget.cpp`

- [ ] **Step 1: Update frontend tests to assert ConfigManager/ExternalAPI state**

Replace direct calls such as:

```cpp
LinkageEngine::instance().disableAction(eventId, actionName, isActive);
LinkageEngine::instance().isActionDisabled(eventId, actionName, isActive);
```

with:

```cpp
ExternalAPI::instance().setEventActionEnabled(eventId, actionName, isActive, false);
!ExternalAPI::instance().isEventActionEnabled(eventId, actionName, isActive);
```

For positive assertions use `ExternalAPI::instance().isEventActionEnabled(...)`. Update every old switch API occurrence in `tests/test_alarm_catalog_widget.cpp`; keep linkage definition/query calls on `LinkageEngine`.

- [ ] **Step 2: Run tests and verify RED**

Run:

```bash
cd /tmp/eventmgr-catalog-test
qmake /home/ff/EventMgr/tests/alarm_catalog_widget_test.pro
make -j4
QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget -txt
```

Expected: UI apply tests fail until `BackendBridge` routes toggles through the new API.

- [ ] **Step 3: Replace paired bridge methods with a single state setter**

In `frontend/backend_bridge.h`, replace `disableAction` / `enableAction` with:

```cpp
void setEventActionEnabled(const QString& eventId,
                           const QString& actionName,
                           bool isActive,
                           bool enabled);
```

In `frontend/backend_bridge.cpp`, replace both implementations with:

```cpp
void BackendBridge::setEventActionEnabled(const QString& eventId,
                                          const QString& actionName,
                                          bool isActive,
                                          bool enabled) {
    ExternalAPI::instance().setEventActionEnabled(
        eventId.toStdString(), actionName.toStdString(), isActive, enabled);
}
```

- [ ] **Step 4: Simplify action diff application**

In `frontend/alarm_catalog_widget.cpp`, replace the branch in `applyActionDiffs()`:

```cpp
if (action.value()) {
    bridge_->enableAction(eventId, action.key(), isActive);
} else {
    bridge_->disableAction(eventId, action.key(), isActive);
}
```

with:

```cpp
bridge_->setEventActionEnabled(
    eventId, action.key(), isActive, action.value());
```

Keep `PendingEventConfig`, live-membership validation, and batching unchanged.

- [ ] **Step 5: Run full frontend tests and verify GREEN**

Run:

```bash
cd /tmp/eventmgr-catalog-test
qmake /home/ff/EventMgr/tests/alarm_catalog_widget_test.pro
make -j4
QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget -txt
```

Expected: all tests pass with zero failures.

- [ ] **Step 6: Commit bridge/UI migration**

```bash
git add frontend/backend_bridge.h frontend/backend_bridge.cpp \
        frontend/alarm_catalog_widget.cpp tests/test_alarm_catalog_widget.cpp
git commit -m "refactor: 前端通过 ExternalAPI 配置联动开关"
```

### Task 5: Verify Integration and Clean Stale References

**Files:**
- Verify: all modified files
- Potential comment-only updates: `backend/config_manager.h`, `backend/linkage_engine.h`, `frontend/backend_bridge.h`

- [ ] **Step 1: Search for stale internal switch APIs**

Run:

```bash
cd /home/ff/EventMgr
grep -RInE 'disableAction|enableAction|isActionDisabled|disabledActive_|disabledClear_|makeDisableKey' \
  backend frontend tests --include='*.h' --include='*.cpp'
```

Expected: no output. If comments mention obsolete ownership, update them to state that `ConfigManager` owns switches.

- [ ] **Step 2: Run LinkageEngine test suite**

Run:

```bash
cd /tmp/eventmgr-linkage-test
qmake /home/ff/EventMgr/tests/linkage_engine_test.pro
make -j4
./test_linkage_engine -txt
```

Expected: zero failed tests.

- [ ] **Step 3: Run alarm catalog test suite**

Run:

```bash
cd /tmp/eventmgr-catalog-test
qmake /home/ff/EventMgr/tests/alarm_catalog_widget_test.pro
make -j4
QT_QPA_PLATFORM=offscreen ./test_alarm_catalog_widget -txt
```

Expected: zero failed tests.

- [ ] **Step 4: Build complete frontend**

Use the existing ignored build directory:

```bash
cd /home/ff/EventMgr/build-frontend-unknown-Debug
qmake ../frontend/frontend.pro
make -j4
```

Expected: exit code 0 and `EventMgrFrontend` linked successfully.

- [ ] **Step 5: Verify diff boundaries**

Run:

```bash
cd /home/ff/EventMgr
git diff --check
git status --short
git diff -- backend/external_api.cpp
```

Expected: feature files are committed. The pre-existing clear-event simplification remains unstaged in `backend/external_api.cpp`; `build/test_eventmgr` and `frontend/frontend.pro.user` remain untouched.

- [ ] **Step 6: Commit any final comment cleanup**

Only if Step 1 required comment edits:

```bash
git add backend/config_manager.h backend/linkage_engine.h frontend/backend_bridge.h
git commit -m "docs: 更新联动开关配置归属注释"
```

Do not push unless explicitly requested.
