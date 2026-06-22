#ifndef SETUP_H
#define SETUP_H

class LinkageEngine;

// ============================================================
// 初始化：为 LinkageEngine 注册各联动类型的 handler
//
// 新增联动类型时在此函数中加一行 registerHandler 即可
// ============================================================
void registerLinkageHandlers(LinkageEngine& engine);

#endif // SETUP_H
