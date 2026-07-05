# 事件管理中心 (EventMgr)

上位机事件管理子系统 — 接收下位机报警信息，进行事件编号、分级、联动、记录和 UI 展示。

## 运行环境

| 项目 | 规格 |
|------|------|
| CPU | 飞腾 FT/2000 |
| 操作系统 | 银河麒麟 V10 |
| 编译器 | GCC (C++11) |
| 前端 | Qt 5.15.2 |

## 架构

```
┌─────────────────────────────────────────┐
│              前端 (Qt)                   │
│  EventMgrWidget                          │
│   ├── EventListWidget    活跃事件列表     │
│   ├── AlarmCatalogWidget 报警配置         │
│   └── AlarmLogWidget     实时报警日志     │
├─────────────────────────────────────────┤
│           BackendBridge (桥接层)          │
├─────────────────────────────────────────┤
│              后端 (C++11)                │
│  ExternalAPI  ──  对外门面               │
│  EventManager ──  事件存储与生命周期      │
│  ConfigManager──  降级/屏蔽配置           │
│  LinkageEngine──  联动执行引擎            │
│  ActionRegistry─  能力集中注册            │
└─────────────────────────────────────────┘
```

- **后端** 纯 C++11，不依赖 Qt
- **前端** Qt 5.15.2，可嵌入任意 QWidget 父级
- **前后端通信** 通过 BackendBridge 桥接（一体模式），可切换为 Socket 代理（分离模式）

## 核心概念

| 术语 | 说明 |
|------|------|
| EventId | 事件编号，格式 `"protocolID-frameID-报警字段"`，全局唯一 |
| 联动 | 告警产生/消除时自动执行的管控动作（如停锅炉、停冷却塔） |
| 降级 | 将高等级报警降低为"提示"，不触发联动 |
| 屏蔽 | 隐藏 UI 报警展示，但联动正常执行 |

## 快速开始

```cpp
#include "backend/event_mgr_module.h"

int main() {
    // 一键启动
    EventMgrModule::init();
    ExternalAPI& api = EventMgrModule::api();

    // observe 组件对接：报警位变化时调用
    api.triggerAlarm(1, 3, "temp_high", true);   // 产生事件
    api.triggerAlarm(1, 3, "temp_high", false);  // 消除事件

    // 手动创建事件
    Event ev = api.createAlarm(1, 3, "temp_high",
        EventLevel::Emergency, "下位机1-温度过高");
    api.addEvent(ev);

    // 查询
    std::vector<Event> active = api.getActiveEvents();
}
```

## 注册联动动作

```cpp
// ActionRegistry::setup() 中集中注册
engine.registerAction("boiler_stop", []() {
    // 调用下位机控制接口
});
engine.registerAction("cooler_fan", []() {
    // 调节冷却塔风机频率
});

// 为具体事件绑定动作
engine.configureEvent("1-3-temp_high", {"cooler_fan"}, {});
engine.configureEvent("2-1-vibration", {"cooler_stop"}, {});

// 按等级绑定默认动作
engine.setLevelDefault(EventLevel::Emergency, {"cooler_stop"});
```

## 编译

```bash
# 后端（无 Qt 依赖）
g++ -std=c++11 -I backend -I backend/stubs -o test_eventmgr \
    main.cpp backend/*.cpp backend/stubs/*.cpp

# 前端
qmake frontend/frontend.pro
make
```

## 前端控件

| 控件 | 说明 |
|------|------|
| `EventMgrWidget` | 主容器，含 Tab 页切换 |
| `EventListWidget` | 活跃事件表格（降级/屏蔽复选框） |
| `AlarmCatalogWidget` | 报警定义配置页（批量应用） |
| `AlarmLogWidget` | 实时报警日志（时间+描述两列） |

## 文档

| 文档 | 路径 |
|------|------|
| 需求规格说明书 | `docs/superpowers/specs/2026-06-26-software-requirements-spec.md` |
| 设计方案 | `docs/superpowers/specs/2026-06-17-event-mgr-design.md` |
| 软件设计说明 | `docs/superpowers/specs/2026-06-17-software-design-spec.md` |
| ActionRegistry 设计 | `docs/superpowers/specs/2026-06-26-action-registry-design.md` |
| 架构图 | `docs/superpowers/specs/2026-06-25-architecture-diagrams.md` |
| 需求讨论记录 | `docs/superpowers/specs/2026-06-17-requirement-discussion-record.md` |

## 目录结构

```
EventMgr/
├── main.cpp                  # 后端演示入口
├── backend/
│   ├── event_types.h         # 数据类型定义
│   ├── event_manager.h/cpp   # 事件存储与生命周期
│   ├── config_manager.h/cpp  # 降级/屏蔽配置管理
│   ├── linkage_engine.h/cpp  # 联动动作执行引擎
│   ├── action_registry.h/cpp # 能力集中注册
│   ├── external_api.h/cpp    # 对外接口门面
│   ├── event_mgr_module.h/cpp# 一键启动模块
│   └── stubs/                # 桩实现（供早期开发用）
├── frontend/
│   ├── frontend.pro          # Qt 工程文件
│   ├── main.cpp              # 前端演示入口
│   ├── eventmgr_widget.h/cpp # 主容器控件
│   ├── event_list_widget.*   # 活跃事件列表
│   ├── alarm_catalog_widget.*# 报警配置页
│   ├── alarm_log_widget.*    # 实时报警日志
│   └── backend_bridge.h/cpp  # Qt ↔ 后端桥接
└── docs/superpowers/specs/   # 设计文档
```
