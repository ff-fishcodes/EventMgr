# 事件配置与联动控制界面设计

## 1. 背景与讨论结论

2026-07-22，用户要求在事件管理中心前端为每个事件提供降级、屏蔽和联动控制，并能查询该事件的联动列表、分别开关每个联动项。经过需求澄清和可视化原型确认，形成以下结论：

1. 配置对象是当前报警目录中的全部事件，不限于活跃事件。
2. 事件产生和事件消除可能配置完全不同的联动，界面必须展示两个独立列表。
3. 每个动作只在其实际所属阶段显示一个开关，不生成不存在的另一阶段开关。
4. 联动列表展示该事件实际可能执行的完整命名联动，包括等级默认和事件专属联动。
5. 每个阶段内等级默认联动排在事件专属联动之前，但界面不标注动作来源。
6. 等级默认联动接口同时接收产生侧和消除侧列表：`setLevelDefault(level, activeActions, clearActions)`。
7. 降级、屏蔽和联动开关都先在前端暂存，点击“应用配置”后统一提交。
8. 页面采用左侧事件表、右侧联动详情面板的固定分栏布局。
9. `LinkageEngine` 的全部配置容器纳入统一线程保护；锁内生成快照，锁外提交任务和执行回调。

本次设计经过用户逐项确认。浏览器原型用于布局讨论，不作为产品运行资源或源码交付物。

## 2. 目标与非目标

### 2.1 目标

- 在“报警配置”页统一管理目录中每个事件的降级、屏蔽和联动开关。
- 准确区分产生侧联动与消除侧联动。
- 让界面展示与联动引擎的有效解析结果一致。
- 支持产生侧和消除侧等级默认联动。
- 保证同一动作在不同事件、不同阶段的禁用状态互不影响。
- 为 `LinkageEngine` 配置读写提供明确、可验证的互斥保护。
- 保持 C++11、Qt 5.15.10 项目基线和现有同进程桥接架构。

### 2.2 非目标

- 不增加配置持久化，所有配置仍在进程重启后丢失。
- 不提供新增、删除或重新绑定联动动作的 UI；界面只开关后端已有的有效联动。
- 不把 fallback 回调作为命名联动项展示或开关。
- 不把运行时临时产生但不在报警目录中的未知事件加入配置表。
- 不为动态关联设备系统事件自动生成目录项。
- 不解决 `EventMgrModule` 首次并发初始化、默认 GUI 事件通知死锁或进程停机协议等既有问题。
- 不承诺线程池动作的完成顺序。

## 3. 当前实现与问题

### 3.1 已有能力

- `AlarmCatalogWidget` 已展示全量报警目录，并暂存、批量应用“降级为提示”和“屏蔽展示”复选框。
- `BackendBridge` 已提供事件联动查询及按事件、动作、阶段启用或禁用的接口。
- `LinkageEngine` 已保存事件专属产生/消除动作和两套禁用集合。
- `ExternalAPI::getAlarmCatalog()` 已合并设备报警定义、纯系统事件定义和当前降级/屏蔽状态。

### 3.2 现有缺口

- `AlarmCatalogWidget` 没有联动入口和联动列表。
- `getEventActions()` 合并产生/消除动作后丢失动作所属阶段。
- `getEventActions()` 不包含等级默认动作，因此不能表示实际有效联动。
- `setLevelDefault()` 只配置产生侧默认动作，消除侧没有对应能力。
- 有效动作合并与 UI 查询没有统一去重规则，同名默认和专属动作可能出现展示与执行不一致。
- `LinkageEngine` 的动作表、显示名表、事件配置、等级默认、fallback 和禁用集合没有互斥保护。

## 4. 总体架构

```text
AlarmCatalogWidget
  ├─ 左侧 Catalog 表：EventId / 描述 / 等级 / 降级 / 屏蔽 / 联动数
  ├─ 右侧联动面板：产生列表 / 消除列表 / 各动作启用状态
  └─ PendingConfig：跨行保留尚未应用的修改
             │
             v
BackendBridge
  ├─ getCatalog()
  ├─ getEventActionGroups(eventId, originalLevel)
  ├─ set/removeDowngrade()
  ├─ set/clearShield()
  └─ enable/disableAction(eventId, actionName, phase)
             │
             v
LinkageEngine + ConfigManager
  ├─ 事件专属产生/消除动作
  ├─ 等级默认产生/消除动作
  ├─ 每事件、动作、阶段的禁用状态
  └─ configMutex_ 保护配置快照
```

联动规则的合并、排序、去重和启用状态计算全部留在 `LinkageEngine`。Qt 层只转换 DTO、暂存用户选择和提交差异，不复制联动业务规则。

## 5. 后端数据模型与接口

### 5.1 阶段动作项

`LinkageEngine` 新增用于单一阶段的查询项：

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
```

所有标量成员在构造时初始化，避免当前默认构造标量不确定的问题。

### 5.2 事件联动分组

```cpp
struct EventActionGroups {
    std::vector<ActionInfo> activeActions;
    std::vector<ActionInfo> clearActions;

    EventActionGroups();
    ~EventActionGroups();
};
```

查询接口为：

```cpp
EventActionGroups getEventActionGroups(
    const EventId& eventId,
    EventLevel originalLevel) const;
```

旧的合并式 `getEventActions(eventId)` 由新接口替代，并同步更新仓库内全部调用点和文档。当前仓库没有需要同时维持两种返回模型的调用方，因此不保留含混的兼容接口。

### 5.3 双阶段等级默认配置

等级默认存储从单个动作向量改为双列表：

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

设置接口为：

```cpp
void setLevelDefault(
    EventLevel level,
    const std::vector<std::string>& activeActions,
    const std::vector<std::string>& clearActions);
```

调用一次即原子替换该等级的两套默认列表。`ActionRegistry` 和演示调用全部迁移到三参数接口。

### 5.4 有效动作解析

产生侧与消除侧采用同一规则：

1. `Info` 原始等级直接返回空列表，和当前执行短路规则一致。
2. 先读取该原始等级对应阶段的默认动作。
3. 再读取该事件对应阶段的专属动作；无显式事件配置时使用 `Event` 自带动作的执行回退仍保留，但目录查询没有运行时 `Event`，因此只查询已登记配置和等级默认。
4. 按动作内部名去重，保留第一次出现的位置，所以默认动作优先。
5. 按 `eventId + actionName + phase` 读取禁用状态并生成 `enabled`。
6. 未登记显示名时回退显示内部名；未登记回调的名称仍可展示当前配置，但执行路径会按现有规则跳过。

执行解析也采用默认在前、专属在后并去重的结果，避免 UI 只显示一次但运行时重复执行。动作按解析顺序提交到线程池，但线程池并行运行，不承诺开始或完成顺序。

## 6. 线程安全设计

### 6.1 保护范围

`LinkageEngine` 新增：

```cpp
mutable QMutex configMutex_;
```

同一把非递归互斥锁保护以下配置域：

- `actionTable_`
- `displayNames_`
- `eventConfig_`
- 双阶段 `levelDefaults_`
- `fallback_`
- `disabledActive_`
- `disabledClear_`

`QThreadPool` 使用自身线程安全接口，不纳入该配置锁。

### 6.2 锁内快照、锁外执行

- 注册、设置、启用、禁用和查询方法在锁内完成配置读写。
- 有效动作解析在一次锁定中复制名称、启用状态和需要的显示信息，保证分组快照内部一致。
- `executeNames()` 在锁内过滤禁用项并复制 `std::function` 回调，解锁后再创建 `ActionTask` 并提交线程池。
- fallback 在锁内复制，解锁后调用。
- 不在持有 `configMutex_` 时执行任何外部回调。

为避免非递归锁嵌套，公开锁定方法不互相调用；内部使用名称带 `Locked` 后缀的私有辅助函数，并只允许在已经持锁时调用。

### 6.3 `clearAll()` 边界

`clearAll()` 先在不持有配置锁时等待线程池已提交任务完成，再锁定并清空配置。它仍是测试或停机清理接口，调用方必须保证没有新的事件执行与它并发进入；本次不引入完整 shutdown 状态机。

## 7. Qt 桥接模型

`BackendBridge` 将后端分组转换为 Qt 类型：

```cpp
struct ActionEntry {
    QString name;
    QString displayName;
    bool enabled;

    ActionEntry();
    ~ActionEntry();
};

struct EventActionGroups {
    QVector<ActionEntry> activeActions;
    QVector<ActionEntry> clearActions;

    EventActionGroups();
    ~EventActionGroups();
};
```

桥接查询签名为：

```cpp
EventActionGroups getEventActionGroups(
    const QString& eventId,
    int originalLevel) const;
```

现有 `disableAction()` 和 `enableAction()` 保持按 `isActive` 区分阶段。桥接只做类型转换，不重新合并或排序动作。

## 8. 前端界面设计

### 8.1 布局

`AlarmCatalogWidget` 使用水平 `QSplitter`：

- 左侧为目录表和底部“刷新”“应用配置”命令。
- 右侧为当前事件标题、EventId、产生侧动作列表、消除侧动作列表和待应用状态。
- 初始选择第一行；用户选中其他行时右侧立即切换，但保留所有行的待提交状态。
- 右侧设置合理最小宽度，左侧表格可伸缩；窄窗口下允许分隔条调整，不让动作名称覆盖开关。

左侧列为：

| 列 | 内容 |
|---|---|
| 事件编号 | 目录 EventId |
| 报警描述 | 面向操作员的说明 |
| 原始等级 | 四级文字和现有等级颜色 |
| 降级为提示 | 二元复选框 |
| 屏蔽展示 | 二元复选框 |
| 联动 | 产生与消除控制项总数；同名动作若属于两个阶段按两项计算，无动作显示“无” |

### 8.2 右侧联动面板

- “事件产生时”和“事件消除时”分别使用独立列表。
- 每行显示动作显示名、内部名和“启用”复选框。
- 不显示“默认”或“专属”来源标签。
- 列表顺序完全采用后端返回顺序。
- 某阶段无动作时显示“无联动”，不显示空白框或无效开关。
- fallback 不显示。

### 8.3 暂存状态

前端为每个目录事件维护 `PendingEventConfig`，包含：

- 当前降级复选状态；
- 当前屏蔽复选状态；
- 产生侧动作名到启用状态的映射；
- 消除侧动作名到启用状态的映射；
- 是否相对后端快照发生修改。

该类型显式声明构造函数和析构函数，并初始化全部标量。选择其他事件前，当前右侧控件状态已经通过信号写入对应待提交对象，因此切换行不会丢失修改。

### 8.4 应用与刷新

点击“应用配置”后：

1. 比较当前待提交状态与加载时快照。
2. 只提交发生变化的降级、屏蔽和动作开关。
3. 动作开关按产生侧和消除侧调用 `enableAction()` 或 `disableAction()`。
4. 所有同步调用返回后重新加载目录和分组状态。
5. 状态栏显示“配置已应用”；配置仍只保存在内存中。

点击“刷新”或离开页面且存在未应用修改时，提供“应用、放弃、取消”三种选择。没有修改时直接重新读取后端。

## 9. 错误与边界行为

- 现有配置 API 返回 `void`，本次不虚构事务回滚或持久化成功语义；“配置已应用”只表示同步内存调用已返回且重新读取完成。
- 动作开关只来自后端查询结果，UI 不允许输入任意动作名称。
- 未注册回调的已配置动作显示内部名，开关仍可修改；执行时继续按现有规则跳过未注册回调。
- 当前报警目录包括设备报警定义和纯系统事件定义；未知运行时事件及动态关联设备系统事件不在表内。
- 降级仍固定设置为 `Info`，不改变联动资格；联动继续按 `originalLevel` 解析。
- 屏蔽仍只影响前端展示或通知，不阻止存储、日志和联动。
- 同一动作在产生和消除两侧出现时拥有两个独立开关。
- 同一默认动作对不同事件的开关按 EventId 隔离，不修改等级默认定义本身。

## 10. 构造、析构与注释要求

- 新增后端 DTO、私有等级配置类型、桥接 DTO 和前端待提交类型均显式声明构造函数、析构函数并初始化标量。
- 修改到的 `AlarmCatalogWidget` 显式声明析构函数。
- 匿名 `ActionTask` 若在本次改动中调整，也补充显式析构函数。
- 对有效动作合并顺序、锁内快照、锁外回调和批量应用状态转换补充必要注释。
- 不添加描述显而易见赋值的冗余注释。

## 11. 第三方库与构建

### 11.1 版本

- 语言标准：C++11。
- 项目目标 Qt：5.15.10。
- 现有前端模块：Qt Core、Gui、Widgets、Concurrent。
- 自动化测试新增使用 Qt Test，目标版本与项目 Qt 版本一致，为 Qt 5.15.10。
- 本机验证版本必须单独记录，不得替代项目目标版本。

### 11.2 构建方式

完整前端继续使用：

```bash
qmake frontend/frontend.pro
make -j4
```

测试工程使用独立 qmake 文件，至少链接 Qt Core、Widgets、Test 及当前后端源码。具体命令在实施计划中给出，并在仓库外临时构建目录执行，避免覆盖用户构建产物。

## 12. 测试设计

### 12.1 后端自动化测试

- 三参数 `setLevelDefault()` 原子保存产生/消除列表。
- 产生和消除分别解析对应等级默认动作。
- 默认动作排在事件专属动作之前。
- 默认与专属同名时只保留并执行一次。
- 同一动作的产生/消除开关独立。
- 同一动作在不同事件上的开关独立。
- `Info` 事件查询为空且不执行动作。
- fallback 在锁外执行并保持现有阶段参数。
- 并发进行动作查询、禁用/启用、事件配置和等级默认配置的压力测试无崩溃、死锁或结构不完整结果。

### 12.2 前端自动化测试

- 目录行包含降级、屏蔽和联动列。
- 选择事件后分别显示产生和消除列表。
- 默认动作排序在前，但界面不显示来源标签。
- 无联动阶段显示空状态。
- 切换事件行后待提交开关保持。
- 点击应用后，降级、屏蔽和两阶段禁用状态与后端重新读取结果一致。
- 存在未保存修改时刷新分支按选择执行应用、放弃或取消。

### 12.3 集成与视觉验证

- 后端演示重新编译并运行至正常结束。
- 完整 Qt 前端 qmake/make 编译通过。
- 使用 `QT_QPA_PLATFORM=offscreen` 运行 Qt Test。
- 通过 `QWidget::grab()` 保存桌面宽度和较窄宽度截图，检查分栏、列宽、长动作名称、开关、空列表和按钮无重叠。
- 不启动已知存在同线程通知死锁的事件产生/清除 GUI 路径；视觉验证只覆盖配置页安全路径。

## 13. 文档同步

实现提交前同步更新：

- 当前需求基线；
- 概要设计；
- 详细设计；
- 类图、调用关系图和配置应用时序图；
- 文档讨论与风险记录；
- 项目 README 和文档导航；
- 第三方库版本、测试构建命令和验证结果。
- `.gitignore` 中加入 `.superpowers/`，避免可视化讨论原型进入产品源码提交。

远程推送前必须再次确认上述文档与最终代码一致。

## 14. 验收标准

1. 报警目录中每个事件均可查看降级、屏蔽和联动控制。
2. 产生和消除动作分组准确，不显示不存在阶段的开关。
3. 默认动作在各阶段排在专属动作之前，且界面不显示来源标签。
4. 双阶段等级默认接口、查询和执行均通过自动化测试。
5. 批量应用后三类配置与后端重新读取一致。
6. `LinkageEngine` 配置域所有读写均受互斥保护，外部回调不在持锁状态执行。
7. 并发压力测试、Qt 前端测试、后端演示和前端构建通过。
8. 桌面及窄窗口截图无控件重叠、文字截断或布局跳变。
9. 需求、概要设计、详细设计、图表、讨论记录、README 与实现同步。
10. 不把内存配置、未验证目标平台或既有未整改问题描述为已完成能力。
