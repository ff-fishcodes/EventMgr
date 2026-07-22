# 事件管理中心（EventMgr）

EventMgr 是上位机事件管理子系统的当前进程内实现，接收设备报警与系统事件，完成活跃事件管理、等级显示、屏蔽、联动调度、日志调用和 Qt 界面展示。仓库可运行后端演示和 Qt 前端演示，但报警目录、日志、设备控制、蜂鸣器及 Socket 通知仍包含示例或输出桩，尚不是完整生产集成。

> **使用前必读**：当前默认 GUI 只能用于启动和静态查看，不能作为功能验收或生产运行入口。GUI 线程中，未屏蔽事件的加入或已命中事件的清除会在 `EventManager` 持有非递归互斥锁时同步触发刷新，刷新重入查询同一把锁后会死锁。接入方必须先隔离这条回调路径，且仍需评估全局回调寿命、联动并发和桩模块等风险。证据与整改状态见[详细设计的并发矩阵](docs/superpowers/specs/2026-07-21-detailed-design.md#concurrency-matrix)和[讨论及风险台账](docs/superpowers/specs/2026-07-21-documentation-discussion-record.md#5-审计风险与整改建议)。

## 环境与依赖

| 项目 | 当前说明 |
|------|----------|
| 语言标准 | C++11 |
| 目标平台 | 项目声明为飞腾 FT/2000 + 银河麒麟 V10；本仓库尚未在目标平台验证 |
| 项目 Qt 目标版本 | Qt 5.15.10 |
| 本地已验证环境 | Qt 5.15.3、GCC 11.4.0（x86_64 Ubuntu） |
| 后端直接源码依赖 | Qt5Core；使用 `QMutex`、`QThreadPool` 等类型 |
| 完整前端工程依赖 | `frontend.pro` 声明 Core、Gui、Widgets、Concurrent |

构建前需准备 `g++`、`pkg-config`、`qmake`、`make`，以及 Qt 5 的开发头文件和库。后端至少需要 Qt5Core；完整前端还需要 Qt5Gui、Qt5Widgets 和 Qt5Concurrent。先确认实际工具链版本：

```bash
g++ --version
pkg-config --version
pkg-config --modversion Qt5Core Qt5Widgets Qt5Concurrent
qmake --version
make --version
```

工具可用不代表版本已经满足项目目标；应分别核对上表中的目标版本和本地已验证版本。

## 当前架构

```text
Qt Widgets
  EventMgrWidget
    +-- EventListWidget
    +-- AlarmCatalogWidget
    +-- BackendBridge (实例，进程内直接访问)
                    |
                    v
EventMgrModule（静态生命周期入口与单例注册）
  +-- ExternalAPI      对外门面
  +-- EventManager     活跃事件与生命周期
  +-- ConfigManager    降级与屏蔽配置
  +-- LinkageEngine    联动解析与线程池调度
  +-- ActionRegistry   动作注册与示例绑定
                    |
                    v
AlarmCatalog / LogWriter / CmdSender / BuzzerControl / SocketServer（当前桩边界）
```

`EventMgrModule::init()` 创建进程期对象并登记 `ExternalAPI`、`EventManager`、`ConfigManager` 和 `LinkageEngine` 的静态实例访问。`BackendBridge` 本身是由 Qt 父对象管理的实例，但当前通过这些全局访问点直接调用后端。前后端 Socket 分离仅是规划边界；没有 Socket 客户端、协议或可靠性机制，无回调时的 `SocketServer` 只把通知 JSON 打印到标准输出。

## 事件标识与语义

| 类型 | 实际 EventId | 示例 |
|------|--------------|------|
| 设备事件 | `deviceName-frameID-alarmField` | `锅炉-3-temp_high` |
| 关联设备的系统事件 | `deviceName-0-eventName` | `锅炉-0-comm_lost` |
| 纯系统事件 | `eventName` | `disk_full` |

标识字段没有对 `-` 做转义，`deviceName` 也没有输入校验；含分隔符或非法帧号时，前后端解析和清除定位可能不一致。详见[详细设计的标识规则](docs/superpowers/specs/2026-07-21-detailed-design.md#id-format)。

- **降级**：事件加入时改变存储和显示使用的 `effectiveLevel`；联动资格和等级默认动作始终使用 `originalLevel`，所以降级不会解除管控。事件已经活跃后再设置降级，只会立即改变配置状态，既有 `Event.effectiveLevel` 不会追溯重算，当前列表的等级文字和颜色可能保持原值。
- **屏蔽**：不阻止事件存储、日志或联动；屏蔽事件的产生侧主动通知被抑制，UI 拉取结果时也会过滤屏蔽项，但命中事件的清除通知无条件发出。
- **动作禁用**：按事件和动作分别控制产生侧、清除侧，仅保存在内存中。活跃事件、降级、屏蔽和动作禁用状态都不会持久化。

## 快速开始

以下是嵌入长期运行宿主进程的集成片段，不是可独立运行后立即退出的完整程序。宿主应在启动阶段初始化一次模块，并从设备采集或系统监控回调中调用 API：

```cpp
#include "backend/event_mgr_module.h"

// 宿主启动阶段调用一次；当前 init() 本身不保证并发首次调用安全。
void initializeEventSubsystem() {
    EventMgrModule::init();
}

// 设备采集回调：deviceName、frameID、alarmField、是否产生。
void onAlarmBitChanged(bool isActive) {
    EventMgrModule::api().triggerAlarm(
        "锅炉", 3, "temp_high", isActive);
}

// 业务代码也可手工创建并加入设备事件。
void reportCustomAlarm() {
    ExternalAPI& api = EventMgrModule::api();
    Event event = api.createAlarm("锅炉", 3, "temp_high",
                                  EventLevel::Emergency, "锅炉温度过高");
    api.addEvent(event);
}

// 系统监控回调；事件名必须来自当前预定义表。
void onSystemStateChanged() {
    ExternalAPI& api = EventMgrModule::api();
    api.triggerSystemEvent("disk_full", true);          // eventName, isActive
    api.triggerSystemEvent("comm_lost", "锅炉", true);
    // 上一行参数依次为 eventName、deviceName、isActive。
}
```

核心对象采用进程期静态访问，当前没有 shutdown/release 接口；宿主必须维持模块及回调依赖对象的有效期，并自行定义进程退出和线程池收尾策略。根目录 `main.cpp` 会固定等待 5 秒后退出，仅用于观察桩动作和线程池的演示，不是宿主生命周期范例。

系统事件名必须来自 `backend/system_events.cpp` 的当前预定义表：

| eventName | 描述 | 原始等级 |
|-----------|------|----------|
| `comm_lost` | 下位机通信断连 | Emergency |
| `comm_restore` | 下位机通信恢复 | Info |
| `disk_full` | 磁盘空间不足 | Serious |
| `cpu_overload` | CPU 过载 | Serious |
| `service_error` | 关键服务异常 | Emergency |

## 联动配置

动作使用内部名、显示名和回调三个参数注册，再用真实 EventId 绑定产生/清除动作。以下片段包含模块初始化、引擎获取和回调声明，可放入宿主的单线程启动阶段：

```cpp
#include "backend/event_mgr_module.h"
#include "backend/linkage_engine.h"
#include "backend/stubs/cmd_sender.h"

void configureSiteLinkage() {
    EventMgrModule::init();
    LinkageEngine& engine = LinkageEngine::instance();

    LinkageEngine::ActionCallback fanCallback = []() {
        CmdSender::send("冷却塔", "set_fan_speed", "1200");
    };
    LinkageEngine::ActionCallback stopCallback = []() {
        CmdSender::send("冷却塔", "emergency_stop", "immediate");
    };

    engine.registerAction("site_cooler_fan", "调风扇", fanCallback);
    engine.registerAction("site_cooler_stop", "停冷却塔", stopCallback);

    engine.configureEvent("锅炉-3-temp_high", {"site_cooler_fan"}, {});
    engine.setLevelDefault(EventLevel::Emergency, {"site_cooler_stop"});

    // 可选：非 Info 原始等级事件提交动作后同步执行 fallback。
    LinkageEngine::FallbackCallback fallback =
        [](const std::string& eventId, bool isActive) {
            // 转交统一兜底管控；生产代码应定义异常与阻塞策略。
        };
    engine.setFallback(fallback);
}
```

`configureEvent` 的显式列表按事件 ID 生效；等级默认动作和是否允许联动都依据 `originalLevel`。可选 fallback 不以动作是否成功解析为条件，会在每个非 `Info` 原始等级事件的产生侧和清除侧同步执行。当前 `ActionRegistry::setup()` 提供完整示例注册，控制与蜂鸣器回调仍调用桩。

## 构建

在仓库根目录构建后端演示：

```bash
mkdir -p build-backend
g++ -std=c++11 -fPIC -no-pie -pthread \
    -I backend -I backend/stubs \
    main.cpp backend/*.cpp backend/stubs/*.cpp \
    $(pkg-config --cflags --libs Qt5Core) \
    -o build-backend/eventmgr-demo
./build-backend/eventmgr-demo
```

`-fPIC -no-pie -pthread` 是本地 Qt reduce-relocations 配置和工具链下的可复现参数；其他工具链是否需要这些选项应以其 Qt 配置为准。

完整 Qt 前端采用独立构建目录：

```bash
mkdir -p build-frontend
cd build-frontend
qmake ../frontend/frontend.pro
make -j4
./EventMgrFrontend
```

上述结果不能替代飞腾 FT/2000、银河麒麟 V10 或项目目标 Qt 5.15.10 上的编译与运行验收。

## 前端控件

| 控件 | 当前用途 |
|------|----------|
| `EventMgrWidget` | 默认主容器，只创建“事件列表”和“报警配置”两个页签 |
| `EventListWidget` | 活跃事件表格，显示有效等级并提供降级、屏蔽入口 |
| `AlarmCatalogWidget` | 报警目录及批量降级、屏蔽配置 |
| `AlarmLogWidget` | 可单独嵌入的当前活跃事件视图，不由默认 `EventMgrWidget` 创建 |

## 文档

[文档中心](docs/README.md)区分当前基线与历史资料。以下内容是当前代码审计形成并确认的 2026-07-21 基线；旧版需求与设计请只通过文档中心追溯。

| 文档 | 内容 |
|------|------|
| [当前需求基线](docs/superpowers/specs/2026-07-21-software-requirements-baseline.md) | 已实现行为、约束与需求差异 |
| [概要设计](docs/superpowers/specs/2026-07-21-high-level-design.md) | 系统边界、模块、数据流与部署状态 |
| [详细设计](docs/superpowers/specs/2026-07-21-detailed-design.md) | 类型、接口、算法、所有权与并发设计 |
| [架构与行为图](docs/superpowers/specs/2026-07-21-architecture-diagrams.md) | 类图、调用关系图与主要时序图 |
| [讨论与审计记录](docs/superpowers/specs/2026-07-21-documentation-discussion-record.md) | 关键讨论、验证证据、风险及后续建议 |
| [文档基线设计方案](docs/superpowers/specs/2026-07-21-documentation-baseline-design.md) | 已批准的文档范围与一致性规则 |

## 目录结构

```text
EventMgr/
|-- main.cpp                       # 后端演示入口
|-- backend/
|   |-- event_types.h              # 公共事件数据类型
|   |-- event_mgr_module.*         # 进程期初始化与静态注册
|   |-- external_api.*             # 对外 API 门面
|   |-- event_manager.*            # 事件生命周期
|   |-- config_manager.*           # 降级与屏蔽内存配置
|   |-- linkage_engine.*           # 联动注册、解析与调度
|   |-- action_registry.*          # 示例动作与事件绑定
|   |-- system_events.*            # 系统事件预定义表
|   `-- stubs/                     # 外部目录、日志、控制与 Socket 桩
|-- frontend/
|   |-- frontend.pro               # Qt qmake 工程
|   |-- main.cpp                   # Qt 演示入口
|   |-- backend_bridge.*           # 进程内 Qt/后端桥接
|   |-- eventmgr_widget.*          # 默认两页签主容器
|   |-- event_list_widget.*        # 活跃事件列表
|   |-- alarm_catalog_widget.*     # 报警配置页
|   `-- alarm_log_widget.*         # 可独立嵌入视图
|-- docs/README.md                 # 当前与历史文档导航
|-- docs/superpowers/specs/        # 需求、设计、图表与讨论记录
`-- doc/                           # 原始输入与历史材料
```
