#ifndef ACTION_REGISTRY_H
#define ACTION_REGISTRY_H

#include "linkage_engine.h"

// ============================================================
// ActionRegistry — 集中注册所有联动能力
//
// 将散落在各控制器和 setup.cpp 中的注册逻辑集中到一处。
// 控制器只需暴露 public 方法，不需关心注册细节。
// ============================================================
class ActionRegistry {
public:
    // 注册所有能力 + 配置所有事件联动
    static void setup(LinkageEngine& engine);
};

#endif // ACTION_REGISTRY_H
