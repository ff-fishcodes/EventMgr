# 事件管理中心概要设计

| 元数据项 | 值 |
|---|---|
| 文档 ID | `EVENTMGR-HLD-20260721` |
| 版本 | 1.2 |
| 日期/状态 | 2026-07-21 / 当前实现审计快照 |
| 源码审计基线 | `9a24bda74cde1b41ded65d858e4f89d92162a9be` |
| 需求审计基线 | [当前需求基线](./2026-07-21-software-requirements-baseline.md)，由提交 `9a24bda74cde1b41ded65d858e4f89d92162a9be` 引入 |

## 1. 设计范围

本文档描述源码审计基线提交 `9a24bda` 所含后端、前端和构建配置的系统上下文、总体架构、模块边界、核心数据流、部署方式、并发模型、对象所有权、依赖及主要设计决策。“当前实现审计快照”表示对该提交中文件内容的静态审计结论，不表示之后提交自动纳入本文，也不表示目标平台或运行行为已经完成验证。[文档基线设计方案](./2026-07-21-documentation-baseline-design.md)为本轮文档工作的已确认范围。

本文档只解释当前总体设计，不展开逐方法参数、成员和失败分支。实现细节、图示和讨论过程分别由已完成并审查的[详细设计](./2026-07-21-detailed-design.md)、[架构与行为图](./2026-07-21-architecture-diagrams.md)和[讨论与审计记录](./2026-07-21-documentation-discussion-record.md)承载；五份交付文档共同构成[当前文档基线](../../README.md#当前基线)。历史文档和 `doc/requirment.md` 只用于追溯原始目标，不作为当前实现已经具备某项能力的证据。

设计范围包括：

- 设备报警和已登记系统事件的产生、活跃存储、清除、查询、降级、屏蔽、联动调度、通知和日志调用。
- 当前 Qt 前端桥接、活跃事件列表、报警目录配置及实时活跃报警视图。
- 当前使用的外部桩边界和完整工程构建方式。

不在当前实现范围内的能力包括：设备报文接收与协议解析、生产级报警目录、持久化配置或历史日志、真实设备控制、可靠 Socket 通信、进程分离、鉴权及目标平台验收。这些能力不得由桩或历史目标推导为已经完成。

## 2. 系统上下文

| 上下文参与者 | 输入或交互 | 当前边界 |
|---|---|---|
| observe/设备监测调用方 | 通过 `ExternalAPI::triggerAlarm()` 上报设备报警位产生或消除 | 报文接收、协议解析和设备名校验不属于本模块；设备事件 ID 由调用参数拼接 |
| 后端监测调用方 | 通过 `ExternalAPI::triggerSystemEvent()` 上报已登记系统事件 | 事件名必须能由 `findSystemEventDef()` 查到，未知名称仅记警告并忽略 |
| Qt 宿主应用 | 创建 `EventMgrWidget`，通过 `BackendBridge` 查询和配置 | 当前只有同进程直接调用模式；桥接不是 Socket 代理实现 |
| 控制与日志基础设施 | 接收联动、通知、目录和日志调用 | 当前均由 `backend/stubs/` 中的桩或静态示例提供，不具有生产集成保证 |

系统以 `ExternalAPI` 作为后端业务门面，以 `BackendBridge` 作为 Qt 类型与标准 C++ 类型之间的适配层。`EventManager` 维护活跃事件，`ConfigManager` 维护内存配置，`LinkageEngine` 解析并异步执行命名动作。证据：`backend/external_api.h` 中 `ExternalAPI`，`frontend/backend_bridge.cpp` 中 `BackendBridge::initialize()`，`backend/event_manager.h` 中 `activeEvents_`，`backend/config_manager.h` 中 `downgradeMap_`/`shieldSet_`，`backend/linkage_engine.h` 中动作与配置容器。

事件标识遵循当前实现而非旧文档：

- 设备事件：`deviceName-frameID-alarmField`，例如 `锅炉-3-temp_high`。证据：`ExternalAPI::createAlarm()`、`EventManager::makeEventId()`。
- 关联设备系统事件：`deviceName-0-eventName`。证据：`ExternalAPI::triggerSystemEvent(eventName, deviceName, isActive)`。
- 纯系统事件：直接使用 `eventName`，尽管 `backend/event_types.h` 中仍有三段式旧注释。证据：`ExternalAPI::triggerSystemEvent(eventName, isActive)` 对 `event.id` 的赋值。

## 3. 总体架构

当前系统是分层但同进程部署的 Qt/C++11 应用：

| 层次 | 组成 | 主要职责 | 依赖方向 |
|---|---|---|---|
| Qt 展示层 | `EventMgrWidget`、`EventListWidget`、`AlarmCatalogWidget`、`AlarmLogWidget` | 展示活跃事件与目录，采集降级、屏蔽和模拟触发操作 | 只经 `BackendBridge` 访问后端能力 |
| Qt 桥接层 | `BackendBridge` | 初始化后端单例、转换 Qt/STL 类型、注入通知与 fallback 回调、发出 Qt 信号 | 直接访问后端单例；当前不经过 Socket |
| 后端门面与编排层 | `EventMgrModule`、`ExternalAPI`、`ActionRegistry` | 建立对象图、暴露事件入口、装配示例动作及联动配置 | 调用事件、配置、联动核心及外部桩 |
| 后端核心层 | `EventManager`、`ConfigManager`、`LinkageEngine` | 活跃生命周期、内存配置、命名动作解析与调度 | 使用公共定义、Qt5Core 同步/线程池能力及桩接口 |
| 公共定义层 | `event_types`、`system_events` | 定义事件值对象、枚举、ID 别名和系统事件表 | 基于 C++ 标准库 |
| 外部适配桩 | `AlarmCatalog`、`SocketServer`、`LogWriter`、`CmdSender`、`BuzzerControl` | 提供静态示例数据或标准输出/延时模拟 | 是开发桩，不是生产基础设施 |

`EventMgrModule::init()` 是当前对象图的装配入口：依次动态创建配置、联动、事件和门面四个对象，登记各自的静态单例指针，再调用 `ActionRegistry::setup()` 注册示例动作。`BackendBridge::initialize()` 在此基础上为 `EventManager` 注入同步通知回调，并为 `LinkageEngine` 注入同步 fallback。证据：`backend/event_mgr_module.cpp` 中 `EventMgrModule::init()`，`frontend/backend_bridge.cpp` 中 `BackendBridge::initialize()`。

## 4. 模块划分

### 4.1 后端核心

| 模块 | 概要职责 | 主要协作方与证据 |
|---|---|---|
| `EventMgrModule` | 创建四个进程期对象、注册单例并装配动作 | `EventMgrModule::init()`；没有 shutdown/release API |
| `ExternalAPI` | 作为事件创建、产生、清除、查询及目录合并的门面 | 委托 `EventManager`/`ConfigManager`，读取 `AlarmCatalog` 和 `system_events`；见 `backend/external_api.cpp` |
| `EventManager` | 按 ID 维护活跃事件，计算加入时有效等级，编排联动、通知和日志 | `processAddEvent()`、两个 `processClearEvent()`、`getActiveEvents()` |
| `ConfigManager` | 在内存中保存降级映射和屏蔽集合 | `downgradeMap_`、`shieldSet_` 由自身非递归 `QMutex` 保护 |
| `LinkageEngine` | 解析事件动作、等级默认和禁用状态，通过私有线程池调度命名回调 | `resolveActiveNames()`、`resolveClearNames()`、`executeNames()`；线程池上限为 4 |
| `ActionRegistry` | 集中登记当前示例动作、显示名、事件绑定和等级默认 | `ActionRegistry::setup()`；动作最终调用控制桩，不代表真实控制已接入 |

### 4.2 公共定义

| 模块 | 概要职责 | 当前约束 |
|---|---|---|
| `event_types` | 定义 `EventLevel`、`EventState`、`EventSource`、`EventId`、`AlarmDef`、`SystemEventDef` 和 `Event` | `originalLevel` 是联动判断依据；`effectiveLevel` 是加入时计算的显示/配置状态 |
| `system_events` | 保存系统事件静态定义并支持按名称查找 | 当前内容是代码内示例表；未知名称不能进入事件生命周期 |

### 4.3 Qt 前端

| 模块 | 概要职责 | 当前约束 |
|---|---|---|
| `BackendBridge` | 调用后端单例、转换 `QString`/STL 类型、发出 `eventsChanged` 和 `linkageAction` | 当前为进程内直连；模拟入口只接受按 `-` 拆分后恰好三段的 ID |
| `EventMgrWidget` | 创建桥接及两个页签，连接事件变化信号并显示屏蔽计数 | 实际创建 `EventListWidget` 和 `AlarmCatalogWidget`，未创建 `AlarmLogWidget` |
| `EventListWidget` | 每秒及收到信号时拉取活跃事件，显示有效等级并提供配置复选框 | 拉取后跳过屏蔽事件；界面降级目标固定为 `Info(4)` |
| `AlarmCatalogWidget` | 展示合并后的设备/系统目录，批量应用降级与屏蔽 | 目录设备部分来自桩；界面降级目标固定为 `Info(4)` |
| `AlarmLogWidget` | 以两列表格轮询显示当前未屏蔽活跃事件 | 名称虽含“Log”，但不是持久历史日志；当前主控件未装配该控件 |

### 4.4 外部桩

| 模块 | 当前行为 | 不应推导的能力 |
|---|---|---|
| `AlarmCatalog` | 返回代码内静态设备报警定义 | 真实配置加载、动态更新或校验 |
| `SocketServer` | 将通知 JSON 打印到标准输出 | 跨进程代理、可靠传输、重连或鉴权 |
| `LogWriter` | 将日志文本打印到标准输出 | 文件/数据库持久化、轮转或审计保证 |
| `CmdSender` | 打印命令并阻塞约 2 秒模拟 ACK | 真实下位机通信、确认、重试或幂等控制 |
| `BuzzerControl` | 打印蜂鸣参数 | 真实硬件控制及错误反馈 |

## 5. 核心数据流

### 5.1 初始化

1. `EventMgrWidget` 构造 `BackendBridge` 并调用 `initialize()`。
2. `EventMgrModule::init()` 若 `api_` 已存在则返回；否则依次 `new ConfigManager`、`new LinkageEngine`、`new EventManager`、`new ExternalAPI`。
3. 四个实例写入各类静态单例指针，`ActionRegistry::setup()` 注册示例命名动作与配置。
4. `BackendBridge` 向 `EventManager` 注入通知 lambda，向 `LinkageEngine` 注入 fallback lambda；两者捕获桥接对象 `this`。每次 `initialize()` 都覆盖两个进程全局单值回调槽，不会新增独立订阅者。

此流程没有初始化失败返回值、并发初始化保护或逆向释放步骤。证据：`EventMgrModule::init()`、`BackendBridge::initialize()`。

### 5.2 事件产生

设备入口先用 `deviceName-frameID-alarmField` 查询 `AlarmCatalog`；未命中时仍以 `Info` 和字段名构造事件并写警告。系统事件入口先查 `system_events`，未知名称写警告后终止。两类事件最终进入 `EventManager::processAddEvent()`。

`processAddEvent()` 的源码编排是：取得 `EventManager` 非递归锁 -> 按 ID 查重 -> 复制事件并按 `ConfigManager` 当前配置计算 `effectiveLevel` -> 写入时间戳和活跃哈希表 -> 按 `originalLevel` 调度产生联动动作 -> 未屏蔽时同步通知 -> 写发生日志。重复 ID 静默返回，不刷新现有记录。

需要区分源码顺序与默认 UI 路径的实际完成点：未屏蔽事件调用通知回调时，回调同步发出 `eventsChanged`；`EventMgrWidget` 的默认同线程连接立即调用 `EventListWidget::refresh()`，随后 `getActiveEvents()` 重入 `EventManager` 的同一把非递归 `QMutex`。因此该路径在回调中死锁，不能声称发生日志已经执行或调用已经正常返回。屏蔽的产生事件跳过通知，才不会触发这一特定重入路径。证据：`EventManager::processAddEvent()`、`EventManager::notifyFrontend()`、`BackendBridge::initialize()`、`EventMgrWidget::setupUI()`、`EventListWidget::refresh()`。

### 5.3 事件清除

设备事件按三参数重建 ID；纯系统事件可按完整 ID 精确清除，关联设备系统事件的完整 ID 会先由 `ExternalAPI::clearEvent(eventId)` 按前两个连字符解析后走三参数清除。命中事件后，源码编排是：持有 `EventManager` 锁 -> 将状态改为 `Cleared` -> 按 `originalLevel` 调度清除动作 -> 同步通知 -> 写消除日志 -> 从活跃哈希表删除。

默认 UI 同线程接线下，所有命中的清除通知均会经过上述同步回调并在刷新查询时重入锁，因此路径停在通知回调中；不能声称消除日志和 `erase` 已完成。清除路径不检查屏蔽状态，所以已屏蔽事件也受此问题影响。证据：`EventManager::processClearEvent()` 两个重载及前述 UI 连线。

### 5.4 配置、查询与联动

- 降级和屏蔽配置直接由桥接调用 `ConfigManager` 单例。`effectiveLevel` 只在事件加入时计算；修改配置不会重算已活跃事件中的值。
- 活跃查询在 `EventManager` 持锁期间复制事件值，桥接随后逐项查询降级和屏蔽状态；返回顺序由 `unordered_map` 决定，不保证稳定。
- 产生联动采用事件产生动作并追加 `originalLevel` 对应的等级默认动作；清除联动只采用清除动作。同名不去重，未注册或已禁用动作不提交。
- 每个可执行动作成为独立 `QRunnable`，异步提交后调用方不等待。若事件原始等级不是 `Info`，fallback 在动作提交循环之后由调用线程同步执行，即使没有任何动作真正提交也会调用。

证据：`ConfigManager::getEffectiveLevel()`，`EventManager::getActiveEvents()`，`LinkageEngine::resolveActiveNames()`、`resolveClearNames()`、`executeActive()`、`executeCleared()`、`executeNames()`。

## 6. 部署与通信模式

当前可运行部署是单进程模式：Qt 展示层、`BackendBridge` 和后端源码共同编译进 `EventMgrFrontend`；桥接经静态单例引用直接调用后端。后端通知通过注入的 C++ 回调同步转成 Qt 信号，联动 fallback 同样通过捕获桥接对象的回调同步发出 `linkageAction`。两个回调槽均是单值而不是订阅列表，因此当前集成隐含“同一时刻只有一个有效桥接”的约束；后初始化的桥接接管两个回调，多个控件/桥接不会获得彼此独立的事件订阅。证据：`frontend/frontend.pro` 的 `SOURCES`、`BackendBridge::initialize()`、`EventManager::setNotifyCallback()`、`LinkageEngine::setFallback()`。

`SocketServer` 只在未注入通知回调时作为打印回退被 `EventManager::notifyFrontend()` 调用。源码注释中的“分离模式下可替换为 Socket 代理”是可演进方向，不是已实现部署模式；仓库没有 Socket 客户端代理、协议握手、重连、鉴权、消息确认或独立后端服务入口。因此当前设计不能宣称支持前后端分进程运行。

飞腾 FT/2000 与银河麒麟 V10 是 `doc/requirment.md` 和项目 README 中的目标约束。仓库没有该平台的构建、运行或兼容性验证记录，本文不将其列为已验证平台。

## 7. 并发与线程模型

当前设计不是单线程模型，也不是使用全局线程池：

| 区域 | 当前同步策略 | 边界与风险 |
|---|---|---|
| `EventManager` | 成员 `QMutex`；增、删、查均以 `QMutexLocker` 持有 | 锁覆盖联动调度、同步 fallback、同步通知和日志调用；`setNotifyCallback()` 不持锁，而执行路径读取 `notifyCb_`，并发设置与读取存在数据竞争 |
| `ConfigManager` | 独立成员 `QMutex`；降级/屏蔽读写均持锁 | 与事件锁是不同锁；配置只保护自身容器，不代表整体事务一致性 |
| `LinkageEngine` | 自有 `QThreadPool linkagePool_`，最大线程数 4 | 动作回调异步并发；动作表、显示名、事件配置、等级默认、`fallback_` 和禁用集合没有互斥保护，`setFallback()` 与执行读取并发时同样存在数据竞争 |
| Qt 展示层 | 当前默认对象在 GUI 线程创建，未指定队列连接 | 当事件也从 GUI 线程触发时，`eventsChanged` 自动连接表现为同线程直接调用；刷新重入事件锁，形成当前默认 UI 死锁。跨线程排队路径不能由此直接推断为死锁 |
| 桩动作 | `CmdSender` 在工作线程中阻塞约 2 秒模拟 ACK | 只验证异步形态，不构成真实性能或可靠性指标 |

动作任务的回调副本由 `ActionTask` 持有，任务设置自动删除；`LinkageEngine::clearAll()` 会先等待线程池完成再清空配置。但常规事件处理不会等待动作完成，动作完成顺序不确定。线程池最大线程数 4 只限制同时运行的任务数，不限制提交数量；当前代码没有设置等待队列上限、背压、取消或陈旧动作淘汰策略，阻塞或慢动作持续到达时任务可能累积。fallback 不在线程池内执行，而是在调度动作后、仍处于 `EventManager` 锁范围内同步执行。

## 8. 生命周期与所有权

| 对象或资源 | 创建与持有 | 释放现状 | 所有权风险 |
|---|---|---|---|
| `ConfigManager` | `EventMgrModule::init()` 动态分配，指针同时保存在 `EventMgrModule::configMgr_` 和单例注册表 | 无 shutdown/release，进程退出前不删除 | 进程期泄漏式所有权；初始化前调用 `instance()`/`config()` 会解引用空指针 |
| `LinkageEngine` | 同上，并被 `EventManager` 引用 | 无模块级删除；自身析构为空 | 线程池与回调生命周期依赖进程退出；配置仅存内存 |
| `EventManager` | 动态分配，持有 `ConfigManager`/`LinkageEngine` 引用 | 无模块级删除 | 引用依赖上述对象先存在；通知回调可捕获寿命更短的桥接对象 |
| `ExternalAPI` | 最后动态分配，持有 `EventManager`/`ConfigManager` 引用 | 无模块级删除 | 单例没有空值保护，依赖固定初始化顺序 |
| `BackendBridge` | `EventMgrWidget` 以 `this` 为 Qt parent 创建 | 随父控件销毁 | 后端单例保存捕获该桥接对象的通知/fallback lambda，桥接析构时没有解除回调，之后触发后端会有悬空捕获风险；后初始化桥接还会覆盖前一桥接的两个回调 |
| 前端页、定时器和多数控件 | 由 Qt parent-child 关系管理 | 随父对象销毁 | `EventMgrWidget` 当前不创建 `AlarmLogWidget`；后者只有被宿主显式创建时才参与运行 |
| `ActionTask` | `executeNames()` 按动作 `new`，交给线程池 | `setAutoDelete(true)` 由线程池执行后删除 | 调用方不拥有任务，也不等待完成 |

四个后端对象的关系由构造函数引用注入建立，但运行期访问主要通过静态单例。`EventMgrModule::init()` 只以 `api_` 是否为空做重复初始化保护，没有并发初始化互斥，也没有所有权封装。证据：`backend/event_mgr_module.cpp`、各核心类 `instance()`/`setInstance()`、`BackendBridge::initialize()`、`ActionTask` 构造函数。

## 9. 第三方依赖与版本

| 依赖 | 项目声明或用途 | 当前本机验证（2026-07-21） | 边界 |
|---|---|---|---|
| C++ | `frontend/frontend.pro` 声明 C++11；后端使用 C++11 标准库 | GCC 11.4.0 | 编译器版本是本机结果，不等同目标平台工具链 |
| Qt | 工程注释声明 Qt 5.15.10 | qmake 3.1，Qt 5.15.3 | 项目声明与本机安装版本分别记录，不混写为同一版本 |
| Qt5Core | `QObject`、`QString`、`QMutex`、`QMutexLocker`、`QThreadPool`、`QRunnable` 等 | 5.15.3 | 后端源码的直接 Qt 依赖是 Qt5Core，不能描述为 Qt-free |
| Qt5Gui/Qt5Widgets | 前端窗口、控件、颜色、布局和 UI 生成代码 | 随本机 Qt 5.15.3 | 前端直接依赖 |
| Qt5Concurrent | `frontend.pro` 声明 `concurrent` | 5.15.3 | 当前后端没有调用 QtConcurrent API，不是后端源码的直接 API 依赖 |
| C++ 标准库 | 容器、函数对象、时间、字符串流、线程休眠等 | 随 GCC 11.4.0 | `std::localtime` 等调用没有额外跨线程保护 |

仓库没有引入其他可识别的包管理依赖。外部桩是项目源文件，不是第三方库。

## 10. 编译与集成方式

前端完整工程使用 qmake/make。为保证 Makefile 生成和相对路径清晰，推荐在前端目录构建：

```bash
cd frontend
qmake frontend.pro
make
```

等价地可在选定构建目录对 `frontend/frontend.pro` 执行 qmake，再在该构建目录执行 make。`frontend.pro` 声明 `QT += core gui widgets concurrent`、`CONFIG += c++11`，并将全部当前后端与桩源码直接编入 `EventMgrFrontend`。这也是当前同进程集成的构建证据。

后端最小源码示例可使用 C++11 并只链接 Qt5Core；`QMutex`、`QThreadPool` 和 `QRunnable` 均由 Qt5Core 提供。本机 Qt 使用 reduce-relocations，验证命令需要 `-fPIC -no-pie`：

```bash
g++ -std=c++11 -fPIC -no-pie -I backend -I backend/stubs \
    main.cpp backend/*.cpp backend/stubs/*.cpp \
    $(pkg-config --cflags --libs Qt5Core)
```

项目 README 当前示例还链接 `Qt5Concurrent`。该命令可作为完整项目/当前示例的宽依赖编译方式，但 `Qt5Concurrent` 不是后端源码的直接 API 必需项。`-fPIC -no-pie` 是否必需取决于所用 Qt 和编译器构建选项；生产集成还需以目标 GCC/Qt 工具链复核 ABI、平台插件和真实外部适配器。飞腾 FT/2000 + 银河麒麟 V10 尚未在本仓库证据中验证。

宿主 Qt 应用的当前集成入口是创建 `EventMgrWidget(parent)`；构造过程自动初始化后端。非 UI 调用方可先调用 `EventMgrModule::init()`，再经 `EventMgrModule::api()` 或已登记单例访问，但必须保证初始化先行。当前没有可用于跨进程部署的构建目标。

## 11. 安全性、可靠性与限制

| 风险 | 当前表现 | 设计影响 |
|---|---|---|
| 默认 UI 重入死锁 | 默认 UI 中事件从 GUI 线程触发，通知在持有 `EventManager` 非递归锁时同步发出，同线程/直接连接刷新后重入查询 | 该条件下未屏蔽产生、所有命中清除可卡死，清除后续日志和删除不会完成；不据此断言跨线程或排队连接也必然死锁 |
| 回调并发与单订阅约束 | `EventManager::notifyCb_` 和 `LinkageEngine::fallback_` 的 setter 均无锁，执行路径也无配套同步；每次桥接初始化覆盖两个单值槽 | 并发设置/读取存在数据竞争；当前只支持一个有效桥接，最后初始化者接管通知和 fallback，多个桥接不独立订阅 |
| 联动配置数据竞争 | `LinkageEngine` 容器和 `fallback_` 无锁 | 不得把注册、配置、查询和执行宣称为可任意跨线程并发 |
| 回调生命周期 | 后端进程期单例保存捕获 `BackendBridge::this` 的 lambda | 当前有效桥接销毁后若后端继续运行，存在悬空访问风险 |
| 内存状态 | 活跃表、降级、屏蔽、联动配置与禁用集合均无持久化 | 重启丢失；无法充当历史审计或配置事实源 |
| ID 解析歧义 | 使用 `-` 拼接；后端按前两个分隔符解析，前端模拟入口要求恰好三段，帧号用宽松转换 | 设备名/字段含 `-` 时行为不一致；非法帧号可能被转换为 0 |
| 禁用键碰撞 | 键为 `eventId + "|" + actionName`，无结构化编码 | 输入包含 `|` 时不同二元组可能产生同一键 |
| 通知 JSON | 使用字符串流手工拼接字段 | 引号、反斜线或控制字符没有可靠转义，不能视为通用安全 JSON 编码 |
| 查找返回值 | `findEvent()` 返回容器元素裸指针后释放锁 | 元素删除或并发访问时不具备稳定生命周期/同步保证 |
| 外部能力为桩 | Socket、日志、目录、命令、蜂鸣器均缺少生产机制 | 无鉴权、可靠投递、持久化、重试、错误恢复或真实设备保证 |
| 异步动作语义与过载 | 任务独立提交，不等待、不聚合结果；私有线程池最多同时运行 4 个任务，但当前代码未设置等待队列上限、背压、取消或陈旧动作策略 | 动作顺序和完成时间不确定；阻塞动作下可能持续积压，也没有失败回传或事务回滚 |
| 生命周期接口缺失 | 核心对象动态分配，无 shutdown/release | 无法有序停止线程池、解除回调或重复建立隔离实例 |

屏蔽只控制产生侧主动通知，并由前端拉取视图再次过滤；它不阻止入表、联动或源码中的日志调用。清除侧没有屏蔽判断。降级只改变 `effectiveLevel` 和显示/config 状态，联动资格与等级默认始终使用 `originalLevel`。这些是当前业务语义，不应按原始需求中的“降级解除管控”解释。

## 12. 设计决策追踪

| 决策编号 | 决策主题 | 当前实现决策与证据 | 历史/原始目标边界 | 状态及代价 |
|---|---|---|---|---|
| HLD-DEC-001 | 全局访问 | `EventMgrModule` 创建对象并由四个核心类保存静态单例指针；桥接使用 `instance()` | 历史提交 `0b26e55` 将核心类改为单例；原始目标未证明需要多实例 | 当前决策；集成简单，但初始化顺序、测试隔离、并发初始化和释放受限 |
| HLD-DEC-002 | 前后端通信 | `BackendBridge` 同进程直调单例并覆盖两个进程全局单值回调 | 历史/原始设计考虑前后端分离；当前 `SocketServer` 只是打印桩 | 当前仅一体、单有效桥接模式；最后初始化者接管回调，Socket 代理/分离模式未实现 |
| HLD-DEC-003 | 活跃存储 | `EventManager` 使用 `unordered_map<EventId, Event>`，按 ID 查重和定位 | 原始目标要求统一管理事件，未要求数据库持久化 | 当前决策；平均常数时间定位、无顺序保证，状态随进程消失 |
| HLD-DEC-004 | 配置存储 | `ConfigManager` 用内存映射/集合保存降级和屏蔽，联动禁用也存内存 | 原始需求关注配置能力；没有当前持久化实现证据 | 当前决策；实现轻量，重启丢失且无跨进程共享 |
| HLD-DEC-005 | 联动扩展 | `LinkageEngine` 以字符串名称登记 `std::function` 回调，事件配置保存动作名 | 历史提交 `c22e69c` 引入命名注册，`ActionRegistry` 集中装配 | 当前决策；解耦事件与调用点，但名称缺少编译期校验，配置容器无锁 |
| HLD-DEC-006 | 动作执行 | 每个动作提交到 `LinkageEngine` 私有 4 线程池，fallback 在提交后同步调用 | 早期历史设计中的串行/单线程描述不再代表当前实现 | 当前并发决策；4 仅为运行并发上限，代码未设队列上限、背压或取消，也无结果聚合、顺序或完成时限保证 |
| HLD-DEC-007 | 等级语义 | `effectiveLevel` 用于加入时的降级结果和展示；联动始终依据 `originalLevel` | 原始需求曾把降级与解除管控关联；历史提交 `bfb8674` 明确改变语义 | 当前决策；显示降级不削弱联动，必须避免用户误解 |
| HLD-DEC-008 | 持久化 | 活跃事件、配置、禁用和日志均没有持久化路径，`LogWriter` 仅打印 | “记录事件/支持排查”是原始目标，不等于当前已有历史库 | 当前限制；重启后无恢复和历史审计能力 |
| HLD-DEC-009 | 标识格式 | 设备使用 `deviceName-frameID-alarmField`，纯系统事件使用单段名称 | 历史提交 `637e49e` 由 `protocolID` 改为 `deviceName`；部分旧注释仍不一致 | 当前决策；人类可读，但分隔符未转义导致解析限制 |
| HLD-DEC-010 | 生命周期 | 四个后端对象按进程期动态分配，没有关闭或释放 API | 这是当前实现结果，并非已确认的长期资源管理目标 | 当前限制；回调解除和有序停机没有设计闭环 |

上述决策表只追踪当前代码已经体现的选择及其来源，不把历史说明中的设想升级为承诺。后续若修复锁范围、引入持久化、实现 Socket 代理或调整等级语义，应同步更新[当前需求基线](./2026-07-21-software-requirements-baseline.md)、本概要设计、[详细设计](./2026-07-21-detailed-design.md)与[架构和行为图](./2026-07-21-architecture-diagrams.md)，并在[讨论与审计记录](./2026-07-21-documentation-discussion-record.md)中保存方案比较和确认过程。
