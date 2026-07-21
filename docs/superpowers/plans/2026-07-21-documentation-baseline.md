# EventMgr Documentation Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立一套与 2026-07-21 当前代码一致、可从 README 导航、包含概要设计、详细设计和架构图的 EventMgr 文档基线。

**Architecture:** 保留既有历史文档，新增带日期的当前基线文档，并以 `README.md` 作为统一入口。所有事实从当前头文件、实现文件和 `frontend/frontend.pro` 提取；原始需求差异与代码风险在讨论记录和需求基线中单独标识，不修改业务代码。

**Tech Stack:** Markdown、Mermaid、C++11、Qt 5.15.10、qmake、GCC、Git、ripgrep

---

## 文件结构

| 文件 | 职责 |
|------|------|
| `README.md` | 项目入口、快速使用、构建方式、当前文档导航 |
| `docs/README.md` | 当前基线与历史文档的分类索引、阅读顺序、版本说明 |
| `docs/superpowers/specs/2026-07-21-software-requirements-baseline.md` | 当前代码已实现行为的需求基线及与原始需求的差异 |
| `docs/superpowers/specs/2026-07-21-high-level-design.md` | 系统上下文、模块分层、部署、数据流、依赖和总体设计决策 |
| `docs/superpowers/specs/2026-07-21-detailed-design.md` | 类型、类、接口、成员、构造析构、算法、线程和失败边界 |
| `docs/superpowers/specs/2026-07-21-architecture-diagrams.md` | 类图、调用关系图和主要业务时序图 |
| `docs/superpowers/specs/2026-07-21-documentation-discussion-record.md` | 本次讨论、方案选择、审计发现、风险分级和后续建议 |

### Task 1: 建立文档导航和版本边界

**Files:**
- Create: `docs/README.md`
- Reference: `docs/superpowers/specs/2026-07-21-documentation-baseline-design.md`
- Reference: `docs/superpowers/specs/2026-07-06-software-requirements-spec.md`
- Reference: `docs/superpowers/specs/2026-07-06-software-design-spec.md`

- [ ] **Step 1: 列出已跟踪文档并确认当前/历史分组**

Run:

```bash
git ls-files 'docs/**' | sort
```

Expected: 输出既有 2026-06、2026-07-06 文档，以及 2026-07-21 文档基线设计方案。

- [ ] **Step 2: 创建文档导航**

写入 `docs/README.md`，固定采用以下章节和分类：

```markdown
# EventMgr 文档中心

## 当前基线
## 推荐阅读顺序
## 历史文档
## 文档维护规则
## 状态标识
```

“当前基线”必须链接本计划定义的五份 2026-07-21 交付文档和已确认的文档基线设计方案；“历史文档”必须说明旧文档用于追溯，不代表当前实现。

- [ ] **Step 3: 验证导航不引用不存在的文件**

Run:

```bash
rg -o '`[^`]+\.md`' docs/README.md
```

Expected: 每个反引号包围的 Markdown 路径都属于本计划的创建文件或 Git 已跟踪文件。

- [ ] **Step 4: 提交文档导航**

```bash
git add docs/README.md
git commit -m "docs: 建立 EventMgr 文档版本导航"
```

### Task 2: 编写当前需求基线

**Files:**
- Create: `docs/superpowers/specs/2026-07-21-software-requirements-baseline.md`
- Reference: `doc/requirment.md`
- Reference: `backend/event_types.h`
- Reference: `backend/external_api.h`
- Reference: `backend/config_manager.h`
- Reference: `backend/linkage_engine.h`
- Reference: `frontend/backend_bridge.h`

- [ ] **Step 1: 创建需求基线骨架**

文档必须包含以下完整章节：

```markdown
# 事件管理中心当前需求基线

## 1. 文档说明
## 2. 系统范围
## 3. 运行与依赖约束
## 4. 数据与标识规则
## 5. 功能需求
### 5.1 设备告警生命周期
### 5.2 系统事件生命周期
### 5.3 活跃事件查询
### 5.4 告警降级
### 5.5 告警屏蔽
### 5.6 联动动作执行
### 5.7 联动动作禁用
### 5.8 前端展示与配置
### 5.9 日志与通知
## 6. 非功能约束
## 7. 当前限制
## 8. 原始需求差异
## 9. 验收追踪
```

- [ ] **Step 2: 用编号需求描述当前可验证行为**

需求编号使用 `CB-` 前缀。至少覆盖以下事实：

```text
CB-ID-001 设备事件 ID 为 deviceName-frameID-alarmField。
CB-ID-002 纯系统事件 ID 当前为 eventName。
CB-LIFE-001 活跃事件按 EventId 查重，重复产生请求静默忽略。
CB-DOWN-001 降级改变 effectiveLevel 和展示颜色，不改变联动使用的 originalLevel。
CB-SHIELD-001 屏蔽不影响存储、日志和联动，只影响展示或通知。
CB-LINK-001 非提示原始等级事件可执行事件级动作和等级默认动作。
CB-LINK-002 动作禁用按 EventId、动作名称、产生侧或消除侧区分。
CB-PERSIST-001 活跃事件、降级、屏蔽和联动禁用均不持久化。
```

每条需求必须附代码依据路径，不使用无法从当前代码证明的性能数字。

- [ ] **Step 3: 编写差异矩阵**

差异矩阵至少包含以下列：

```markdown
| 原始要求 | 当前实现 | 影响 | 状态 |
|----------|----------|------|------|
```

至少记录 Qt 版本、后端 Qt 依赖、`protocolID`/`deviceName`、降级是否解除管控、单线程/并发模型、Socket 分离是否已实现。

- [ ] **Step 4: 检查术语和占位内容**

Run:

```bash
rg -n 'protocolID|Qt 5\.8\.12|待定|待补充|TODO|TBD' docs/superpowers/specs/2026-07-21-software-requirements-baseline.md
```

Expected: `protocolID` 和 Qt 5.8.12 只出现在原始需求差异中；没有未完成标记。

- [ ] **Step 5: 提交需求基线**

```bash
git add docs/superpowers/specs/2026-07-21-software-requirements-baseline.md
git commit -m "docs: 新增当前实现需求基线"
```

### Task 3: 编写概要设计

**Files:**
- Create: `docs/superpowers/specs/2026-07-21-high-level-design.md`
- Reference: `backend/event_mgr_module.cpp`
- Reference: `backend/external_api.cpp`
- Reference: `backend/event_manager.cpp`
- Reference: `backend/config_manager.cpp`
- Reference: `backend/linkage_engine.cpp`
- Reference: `frontend/backend_bridge.cpp`
- Reference: `frontend/frontend.pro`

- [ ] **Step 1: 创建概要设计章节**

使用以下结构：

```markdown
# 事件管理中心概要设计

## 1. 设计范围
## 2. 系统上下文
## 3. 总体架构
## 4. 模块划分
## 5. 核心数据流
## 6. 部署与通信模式
## 7. 并发与线程模型
## 8. 生命周期与所有权
## 9. 第三方依赖与版本
## 10. 编译与集成方式
## 11. 安全性、可靠性与限制
## 12. 设计决策追踪
```

- [ ] **Step 2: 明确模块边界**

“模块划分”必须区分四组：

```text
后端核心：EventMgrModule、ExternalAPI、EventManager、ConfigManager、LinkageEngine、ActionRegistry
公共定义：event_types、system_events
Qt 前端：BackendBridge、EventMgrWidget、EventListWidget、AlarmCatalogWidget、AlarmLogWidget
外部桩：AlarmCatalog、SocketServer、LogWriter、CmdSender、BuzzerControl
```

不得把桩模块描述为生产实现，不得把预留 Socket 代理描述为已经完成。

- [ ] **Step 3: 记录依赖版本和编译方式**

必须写明：

```text
C++ 标准：C++11
项目声明 Qt 版本：5.15.10
Qt 模块：core、gui、widgets、concurrent
前端：qmake frontend/frontend.pro && make
后端示例：g++ -std=c++11，链接 Qt5Core 与 Qt5Concurrent
目标环境：飞腾 FT/2000、银河麒麟 V10
```

同时说明当前后端直接使用 `QMutex`、`QMutexLocker`、`QThreadPool` 和 `QRunnable`。

- [ ] **Step 4: 核对架构声明**

Run:

```bash
rg -n '单线程|无 Qt|已实现.*Socket|默认使用全局线程池' docs/superpowers/specs/2026-07-21-high-level-design.md
```

Expected: 不出现与当前代码矛盾的肯定性描述；如引用历史设计，必须带“历史”或“原始需求”限定。

- [ ] **Step 5: 提交概要设计**

```bash
git add docs/superpowers/specs/2026-07-21-high-level-design.md
git commit -m "docs: 补充事件管理中心概要设计"
```

### Task 4: 编写详细设计

**Files:**
- Create: `docs/superpowers/specs/2026-07-21-detailed-design.md`
- Reference: `backend/*.h`
- Reference: `backend/*.cpp`
- Reference: `backend/stubs/*.h`
- Reference: `backend/stubs/*.cpp`
- Reference: `frontend/*.h`
- Reference: `frontend/*.cpp`

- [ ] **Step 1: 建立逐模块详细设计结构**

每个类或模块统一使用以下模板：

```markdown
### 类或模块名

**职责**：
**源文件**：
**构造与析构**：
**公开接口**：
**关键成员及所有权**：
**处理流程**：
**并发与线程安全**：
**失败与边界行为**：
```

- [ ] **Step 2: 覆盖公共类型和后端模块**

必须逐项覆盖：

```text
EventLevel、EventState、EventSource、EventId、AlarmDef、SystemEventDef、Event
EventMgrModule、ExternalAPI、EventManager、ConfigManager、LinkageEngine、ActionRegistry
system_events
AlarmCatalog、SocketServer、LogWriter、CmdSender、BuzzerControl
```

`EventManager` 章节必须说明锁覆盖范围、通知 JSON 字段、日志格式、重复事件行为和 `findEvent()` 指针生命周期风险。`LinkageEngine` 章节必须说明四线程池、动作解析顺序、`originalLevel` 语义、fallback 时机、禁用键格式以及缺少显式锁保护的边界。

- [ ] **Step 3: 覆盖 Qt 桥接和前端控件**

必须逐项覆盖：

```text
BackendBridge、EventMgrWidget、EventListWidget、AlarmCatalogWidget、AlarmLogWidget
```

说明 `BackendBridge::initialize()` 的单例初始化、通知回调和 fallback 注入；说明各控件的信号刷新、1 秒定时兜底、屏蔽过滤和配置提交行为。

- [ ] **Step 4: 建立接口汇总表**

接口表必须包含：

```markdown
| 类/模块 | 接口 | 输入 | 输出 | 副作用 | 线程说明 |
|---------|------|------|------|--------|----------|
```

公开方法以头文件为准，禁止记录已删除的 `registerHandler`、数值型 `protocolID` 接口或自动 mirror 解锁行为。

- [ ] **Step 5: 核查构造与析构记录**

Run:

```bash
rg -n 'class |struct |^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*\(' backend/*.h backend/stubs/*.h frontend/*.h
rg -n '构造与析构' docs/superpowers/specs/2026-07-21-detailed-design.md
```

Expected: 所有具有实例生命周期的核心类和 QWidget/QObject 类均有构造与析构现状说明；纯静态工具类明确说明未声明实例构造/析构。

- [ ] **Step 6: 提交详细设计**

```bash
git add docs/superpowers/specs/2026-07-21-detailed-design.md
git commit -m "docs: 补充事件管理中心详细设计"
```

### Task 5: 重建当前架构和时序图

**Files:**
- Create: `docs/superpowers/specs/2026-07-21-architecture-diagrams.md`
- Reference: `backend/event_mgr_module.cpp`
- Reference: `backend/event_manager.cpp`
- Reference: `backend/linkage_engine.cpp`
- Reference: `frontend/backend_bridge.cpp`

- [ ] **Step 1: 创建图表目录**

文档使用以下章节：

```markdown
# 事件管理中心架构与行为图

## 1. 图例与边界
## 2. 类图
## 3. 调用关系图
## 4. 模块初始化时序图
## 5. 设备告警产生时序图
## 6. 告警消除时序图
## 7. 降级与屏蔽配置时序图
## 8. 联动动作异步执行时序图
```

- [ ] **Step 2: 编写当前类图和调用关系图**

类图必须包括核心单例类、Qt 桥接、前端控件和桩模块。关系必须反映：

```text
EventMgrModule 创建并注册四个核心单例
ExternalAPI 委托 EventManager 和 ConfigManager
EventManager 调用 ConfigManager、LinkageEngine、LogWriter、SocketServer/NotifyCallback
BackendBridge 通过 instance() 访问核心单例
LinkageEngine 通过 ActionRegistry 注册的回调调用 CmdSender/BuzzerControl
```

- [ ] **Step 3: 编写六个行为图**

所有 sequenceDiagram 必须展示真实同步/异步边界：事件管线不等待 `ActionTask` 完成；`fallback_` 在动作提交后同步调用；前端通知回调只发出 `eventsChanged()`，不解析 JSON。

- [ ] **Step 4: 检查 Mermaid 语法块和旧术语**

Run:

```bash
test "$(rg -c '^```mermaid$' docs/superpowers/specs/2026-07-21-architecture-diagrams.md)" -eq "$(rg -c '^```$' docs/superpowers/specs/2026-07-21-architecture-diagrams.md)"
rg -n 'registerHandler|protocolID|mirror|BoilerController|CoolingTowerController' docs/superpowers/specs/2026-07-21-architecture-diagrams.md
```

Expected: Mermaid 开闭块数量相等；不出现旧设计类和已删除接口。

- [ ] **Step 5: 提交图表文档**

```bash
git add docs/superpowers/specs/2026-07-21-architecture-diagrams.md
git commit -m "docs: 重建当前架构与调用时序图"
```

### Task 6: 记录讨论、审计发现和整改建议

**Files:**
- Create: `docs/superpowers/specs/2026-07-21-documentation-discussion-record.md`
- Reference: `docs/superpowers/specs/2026-07-21-documentation-baseline-design.md`
- Reference: `doc/requirment.md`

- [ ] **Step 1: 记录本次关键讨论**

讨论记录必须包含：

```text
用户请求：梳理项目文档及代码并完善文档
基线选择：A，以当前代码为准，差异单独记录
组织方案：1，新增当前基线并保留历史版本
范围确认：只改文档，不修改业务代码
交付确认：需求基线、概要设计、详细设计、图表、讨论记录和导航
```

- [ ] **Step 2: 编写风险分级表**

使用以下列：

```markdown
| 编号 | 级别 | 事实 | 影响 | 建议 | 本次处理 |
|------|------|------|------|------|----------|
```

至少纳入事件 ID 分隔符限制、纯系统事件格式差异、`findEvent()` 指针生命周期、`LinkageEngine` 并发保护、模块释放入口、手工 JSON 转义、降级语义冲突和 Socket 分离未落地。

- [ ] **Step 3: 区分事实和建议**

Run:

```bash
rg -n '已实现|建议|本次不修改|待决' docs/superpowers/specs/2026-07-21-documentation-discussion-record.md
```

Expected: 每项整改方向明确标记为建议或待决，不描述成已实现。

- [ ] **Step 4: 提交讨论记录**

```bash
git add docs/superpowers/specs/2026-07-21-documentation-discussion-record.md
git commit -m "docs: 记录文档基线讨论与代码审计结论"
```

### Task 7: 更新项目 README

**Files:**
- Modify: `README.md`
- Reference: `frontend/frontend.pro`
- Reference: `main.cpp`
- Reference: `backend/action_registry.cpp`
- Reference: `docs/README.md`

- [ ] **Step 1: 修正当前术语和示例**

将 README 中事件编号统一为：

```text
设备事件：deviceName-frameID-alarmField，例如 锅炉-3-temp_high
关联设备系统事件：deviceName-0-eventName，例如 锅炉-0-comm_lost
纯系统事件：eventName，例如 disk_full
```

所有 C++ 示例必须使用字符串设备名，并与 `external_api.h` 的签名一致。

- [ ] **Step 2: 修正降级、屏蔽和联动说明**

明确说明：

```text
降级只改变 effectiveLevel 及前端显示，不改变联动决策。
屏蔽不影响日志和联动，前端不展示被屏蔽事件。
联动动作可按产生侧和消除侧分别禁用。
```

- [ ] **Step 3: 更新构建命令和文档导航**

前端命令使用独立构建目录，避免覆盖源码目录：

```bash
mkdir -p build-frontend
cd build-frontend
qmake ../frontend/frontend.pro
make -j4
```

文档表必须首先链接 `docs/README.md` 和五份当前基线文档，再列出历史入口。

- [ ] **Step 4: 检查 README 与代码一致性**

Run:

```bash
rg -n 'protocolID|降级.*不触发联动|Qt 5\.8\.12|registerAction\([^,]+,[^,]+\)' README.md
```

Expected: 不出现旧 EventId 术语、旧降级语义、旧 Qt 版本或两参数 `registerAction` 示例。

- [ ] **Step 5: 提交 README**

```bash
git add README.md
git commit -m "docs: 更新项目入口与当前使用说明"
```

### Task 8: 全量一致性验证

**Files:**
- Verify: `README.md`
- Verify: `docs/README.md`
- Verify: `docs/superpowers/specs/2026-07-21-*.md`

- [ ] **Step 1: 检查格式和占位内容**

Run:

```bash
git diff --check 8d084f3..HEAD
rg -n 'TODO|TBD|待补充|稍后完善' README.md docs/README.md docs/superpowers/specs/2026-07-21-*.md
```

Expected: 两条命令均无输出。

- [ ] **Step 2: 检查术语边界**

Run:

```bash
rg -n 'protocolID|Qt 5\.8\.12|后端不.*Qt|单线程' README.md docs/README.md docs/superpowers/specs/2026-07-21-*.md
```

Expected: 命中只位于原始需求差异、历史说明或审计记录中。

- [ ] **Step 3: 验证项目声明的工具链**

Run:

```bash
qmake -v
g++ --version
pkg-config --modversion Qt5Core Qt5Concurrent
```

Expected: 记录本机实际版本；若与项目声明的 Qt 5.15.10 不同，在验证结果中区分项目基线和本机环境。

- [ ] **Step 4: 编译后端示例**

Run:

```bash
mkdir -p /tmp/eventmgr-doc-verify
g++ -std=c++11 -I. -Ibackend -Ibackend/stubs main.cpp backend/*.cpp backend/stubs/*.cpp -o /tmp/eventmgr-doc-verify/eventmgr $(pkg-config --cflags --libs Qt5Core Qt5Concurrent) -fPIC -pthread
```

Expected: 编译成功，生成 `/tmp/eventmgr-doc-verify/eventmgr`，不修改仓库内构建产物。

- [ ] **Step 5: 运行后端示例**

Run:

```bash
/tmp/eventmgr-doc-verify/eventmgr
```

Expected: 输出六组演示，事件产生/消除、未知事件兜底和异步联动流程均能运行，进程正常退出。

- [ ] **Step 6: 验证前端工程**

Run:

```bash
mkdir -p /tmp/eventmgr-frontend-verify
cd /tmp/eventmgr-frontend-verify
qmake /home/ff/EventMgr/frontend/frontend.pro
make -j4
```

Expected: 生成 `EventMgrFrontend`；验证仅编译，不启动 GUI。

- [ ] **Step 7: 检查最终工作区边界**

Run:

```bash
git status --short
git log --oneline -8
```

Expected: 用户原有的 `build/test_eventmgr`、`frontend/frontend.pro.user` 和 `build-frontend-unknown-Debug/` 保持原状；本计划产生的文档均已提交，提交日志准确概括各阶段主要修改。
