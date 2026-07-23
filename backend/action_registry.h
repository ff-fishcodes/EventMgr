#ifndef ACTION_REGISTRY_H
#define ACTION_REGISTRY_H

// ============================================================
// ActionRegistry — 业务侧注册示例
//
// 演示业务代码如何在 EventMgrModule::init() 之后，通过 ExternalAPI 门面
// 注册联动能力、配置事件联动、登记系统事件定义。
// 不依赖 LinkageEngine，业务代码对其无感知。
// 实际项目中此文件由业务方按需替换。
// ============================================================
class ActionRegistry {
public:
    // 通过 ExternalAPI 单例注册所有能力/事件/系统事件
    static void setup();
};

#endif // ACTION_REGISTRY_H
