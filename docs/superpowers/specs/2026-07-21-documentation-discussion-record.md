# EventMgr 文档基线讨论与代码审计记录

| 元数据项 | 内容 |
|---|---|
| 文档 ID | `EVENTMGR-DOC-AUDIT-20260721` |
| 版本 | 1.3 |
| 日期 | 2026-07-21 创建；2026-07-22 完成后状态补录（Asia/Shanghai） |
| 状态 | 文档基线与 README 已完成自动化验证和评审，当前状态以[文档中心](../../README.md)为准；这不是用户验收或产品批准；19 项代码风险均未实施 |
| 审计源码基线 | `9a24bda74cde1b41ded65d858e4f89d92162a9be` |
| 文档分支/隔离目录 | `docs/documentation-baseline` / `/home/ff/EventMgr/.worktrees/documentation-baseline` |
| 记录性质 | 对讨论结论、代码/文档审计证据和评审处置的摘要；除特别标明外均为意译，不是逐字对话 |

## 1. 目的与边界

本文用于满足项目对关键需求讨论、需求澄清和设计变更进行可追溯记录的要求。记录对象是“梳理项目文档及代码并完善文档”这一轮工作的方案选择、事实基线、审计发现、文档评审修订、验证环境和后续建议。

本轮已确认只修改文档，不修改业务代码。因此：

- “当前事实”表示可从审计源码基线、构建配置或实际命令输出核验的状态。
- “文档已处理”表示风险和边界已纳入本轮文档，不表示对应代码缺陷已修复。
- “建议”“待产品确认”“未实施”表示后续工作方向，不构成本轮实现承诺。

关联入口：[文档中心](../../README.md)、[文档基线设计方案](./2026-07-21-documentation-baseline-design.md)、[执行计划](../plans/2026-07-21-documentation-baseline.md)、[当前需求基线](./2026-07-21-software-requirements-baseline.md)、[概要设计](./2026-07-21-high-level-design.md)、[详细设计](./2026-07-21-detailed-design.md)、[架构与行为图](./2026-07-21-architecture-diagrams.md)和[项目 README](../../../README.md)。

## 2. 讨论时间线与决策日志

以下内容是对已确认讨论的摘要，不声称复现逐字原话。

| 顺序 | 讨论项 | 选项与结论 | 决策理由/影响 |
|---|---|---|---|
| 1 | 用户请求 | 梳理项目文档及代码并完善文档 | 需要同时审计代码事实和历史文档，不能仅做格式整理 |
| 2 | 事实基线 | A：以当前代码为事实，差异单独记录；B：以原始需求为基准列整改清单；C：同步修改文档与代码使二者收敛。用户选择 A | 避免把原始目标误写为已实现能力，同时保留整改入口 |
| 3 | 文档组织 | 1：新增当前基线并保留历史；2：覆盖旧文档；3：只增加补充说明。用户选择 1 | 保留版本演进和历史决策；当前读者不必拼接多份过时文档 |
| 4 | 范围确认 | 只完善文档，不修改业务代码 | 当前事实、原始需求差异和整改建议分别记录；代码缺陷一律留作后续工作 |
| 5 | 交付确认 | 需求基线、概要设计、详细设计、架构与行为图、讨论记录、文档导航及根 README | 同时满足需求、设计、图示、讨论追溯和项目入口要求 |
| 6 | 执行方式 | 用户选择子代理按任务执行，并批准使用隔离 worktree | 文档工作在 `docs/documentation-baseline` 分支的隔离目录进行，避免触碰原工作树中的用户改动 |

已确认的设计与计划分别落在：

- [文档基线设计方案](./2026-07-21-documentation-baseline-design.md)，提交 `8d084f30b23ee8db949f5b3355acefc9c48b8180`。
- [文档基线执行计划](../plans/2026-07-21-documentation-baseline.md)，提交 `046fd91e98a65eec67eb9ddad37d37d54b25e7d5`。
- 提交 `b692a00fc634840b6fc2e6d20290a2559bae3f46` 将本地 `.worktrees/` 加入忽略规则；随后在分支 `docs/documentation-baseline` 和隔离目录中执行 Task 1-8。

## 3. 代码与文档审计方法

### 3.1 证据优先级

1. 审计源码基线中的头文件、实现文件、Qt 工程文件和两个入口程序；用于判定当前存在的接口、调用顺序、锁、所有权和构建依赖。
2. Git 历史；用于解释标识重构、并发、单例化、降级和动作禁用等设计演进，不用历史提交替代当前代码事实。
3. 2026-07-21 当前基线文档；用于交叉核对术语、需求编号、设计边界和图示一致性。
4. 既有正式文档；只作为已记录设计和历史决策的追溯来源。
5. [原始需求](../../../doc/requirment.md)；只作为目标来源，不作为当前已实现能力的证据。

若材料冲突，以当前代码和构建配置为“当前事实”，同时把原始要求、影响、建议和待确认决定单列。静态审计能证明源码结构和潜在路径，但不能替代目标平台运行验证；本机编译成功也不能外推为飞腾 FT/2000 与银河麒麟 V10 已验证。

### 3.2 审计范围与方法

- 按 `backend/`、`backend/stubs/`、`frontend/`、根 `main.cpp` 和 `frontend/frontend.pro` 建立模块清单。
- 从公开头文件反查实现，核对接口参数、返回值、构造/析构、复制限制和 DTO 默认初始化。
- 沿产生、清除、查询、配置、通知、fallback 和异步动作链检查锁范围、同步/异步边界、回调所有权和失败分支。
- 对照 Git 历史和旧文档，建立原始需求差异矩阵；不从注释或桩推导生产能力。
- 使用本机工具链实际编译后端和前端；图表检查 Mermaid 代码块结构和源码名称，因缺少渲染器未做渲染验收。

## 4. 自动化工作流评审处置

本节记录的是自动化代理工作流中的评审与集成处置，不是用户验收或产品批准。用户只批准了第 2 节中的事实基线 A、组织方案 1、只改文档的范围、交付集合、子代理执行和隔离 worktree。文档条目由以下自动化角色处理，无法从仓库取得人类评审者姓名时不虚构姓名：

| 自动化角色 | 职责 | 处置权限边界 |
|---|---|---|
| 实现代理 | 编写任务文档并执行自检 | 可提出修订，不能代表用户或产品接受 |
| 规格合规评审代理 | 对照任务规格检查覆盖、证据和状态措辞 | 可提出“需修订/规格符合”结论，不能批准业务决定 |
| 质量评审代理 | 检查事实准确性、风险遗漏、可复现性和审计可追溯性 | 可提出质量发现，不能把建议改成已实现事实 |
| 控制器 | 汇总代理结论、决定是否纳入文档分支并创建/修订提交 | 仅有自动化工作流集成处置权；不替代用户、产品负责人或代码所有者的验收权 |

仓库未持久保留每轮原始评审 wall-clock 日志、逐条原始评审对话或全部中间候选稿。Task 1-5 的精确时间值实际来自对应纳入提交的 committer timestamp，只作为评审活动已经在该提交前完成的可用时间代理，不声称是评审发生的精确时刻。下表记录可核验的 durable revision、评审摘要和证据路径；不得反推为完整逐字评审记录。

| 任务 | 自动化评审角色 | 纳入提交时间（Asia/Shanghai，作为可用证据时间代理） | 已审/纳入修订 | 发现证据或参考 | 发现与修订摘要 | 自动化处置 | 处置权限 |
|---|---|---|---|---|---|---|---|
| Task 1 文档导航 | 实现代理、规格合规评审代理、质量评审代理、控制器 | 2026-07-21 19:15:49 | `27e162d45e4c503014f15494658fd34e33fdc48e` | [文档中心](../../README.md)及[执行计划 Task 1](../plans/2026-07-21-documentation-baseline.md) | 发现需要区分当前基线、历史资料和计划中链接；增加分类导航、阅读顺序、维护规则和状态定义 | 控制器纳入分支；工作流接受 | 非用户批准 |
| Task 2 需求基线 | 实现代理、规格合规评审代理、质量评审代理、控制器 | 2026-07-21 20:16:44 | `9a24bda74cde1b41ded65d858e4f89d92162a9be` | [当前限制](./2026-07-21-software-requirements-baseline.md#7-当前限制)、[原始需求差异](./2026-07-21-software-requirements-baseline.md#8-原始需求差异) | 补入默认 UI 死锁、回调接管/竞争、无界队列、Qt5Core/Concurrent 边界、`findEvent()` 指针规则和降级非追溯行为；修正节点重哈希指针规则 | 控制器纳入分支；工作流接受 | 非用户批准；产品差异仍待确认 |
| Task 3 概要设计 | 实现代理、规格合规评审代理、质量评审代理、控制器 | 2026-07-21 21:20:52 | `9d0bdd0f347df42dd6d2319a666a1dc59028814b` | [并发模型](./2026-07-21-high-level-design.md#7-并发与线程模型)、[生命周期与所有权](./2026-07-21-high-level-design.md#8-生命周期与所有权) | 补入死锁完成点、单值回调所有权/悬空风险、无界队列、初始化关闭缺口和实际 UI 消费者 | 控制器纳入分支；工作流接受 | 非用户批准 |
| Task 4 详细设计 | 实现代理、规格合规评审代理、质量评审代理、控制器 | 2026-07-21 22:49:30 | `913da30e76b9decd1bba83534ba2ffd849160075` | [公共回调/DTO 索引](./2026-07-21-detailed-design.md#public-type-index)、[并发矩阵](./2026-07-21-detailed-design.md#concurrency-matrix)、[构造析构审计](./2026-07-21-detailed-design.md#ctor-dtor-audit) | 补齐 28 个设计单元、24 个可构造类型、DTO 标量、回调/容器并发、`clearAll()` 等待、本机 Qt5Core-only 编译和六场景行为 | 控制器纳入分支；工作流接受 | 非用户批准；代码整改未实施 |
| Task 5 架构与行为图 | 实现代理、规格合规评审代理、质量评审代理、控制器 | 2026-07-22 00:35:36 | `15483b44a91e4b42ab1be81cd0fb44301229a31d` | [调用关系](./2026-07-21-architecture-diagrams.md#call-views)、[产生时序](./2026-07-21-architecture-diagrams.md#sequence-active)、[异步时序](./2026-07-21-architecture-diagrams.md#sequence-async) | 修正调用方向、fallback 同步位置、UI 重入、屏蔽分支和 UI 消费者边界；Mermaid 结构通过但未渲染 | 控制器纳入分支；工作流接受 | 非用户批准 |
| Task 6 讨论记录 | 实现代理、规格合规评审代理、质量评审代理、控制器 | 2026-07-22；原始评审精确 wall-clock 未持久保留 | durable reviewed candidate `ffd994cd47cef502563473691de4b584cdafbd29` | [风险与整改建议](#5-审计风险与整改建议)、[验证环境](#7-2026-07-21-验证环境与结果)、[后续优先级](#10-后续优先级) | 规格复审补入回调异常和 `localtime` 风险；质量复审要求区分严重度/优先级、补全治理台账、评审权限、环境证据和可复现命令；本次后续处置修正 durable revision 与时间证据语义 | 规格/质量检查通过；本次后续处置提交记录对 candidate 修订的自动化工作流接受 | 非用户批准；控制器仅负责分支集成 |

Task 6 持久化机制：`ffd994cd47cef502563473691de4b584cdafbd29` 是被本次后续处置评审的 durable candidate。本次处置提交本身记录对该 candidate 质量修订的自动化接受；一个提交不能在创建前知道并可靠写入自己的 SHA，因此不得为了把处置提交 SHA 写回本文而继续自我 amend。审计者应使用 `git log --follow -- docs/superpowers/specs/2026-07-21-documentation-discussion-record.md` 查找 candidate 之后修改本文的处置提交，或由仓库维护者使用外部 tag/Git note 标注。处置提交身份存在 Git 历史或外部元数据中，不作为本文自引用字段。上述自动化处置不是用户批准，也不改变任何代码风险的“未实施”状态。

## 5. 审计风险与整改建议

本次共记录 19 项风险。“级别”使用高/中/低，表示风险一旦发生时对正确性、安全性、可用性或交付能力的潜在影响；“整改优先级”在第 5.1 节使用 P0-P3，表示综合默认路径可达性、生产门禁、产品决定和前置依赖后的处理顺序。二者不是同一维度：例如生产适配桩是高严重度，但在产品确认生产边界后、生产集成前处理，列为 P1；这不降低其影响判断。“本次处理”仅记录文档处置状态。

| 编号 | 级别 | 事实 | 影响 | 建议 | 本次处理 |
|---|---|---|---|---|---|
| R-001 | 高 | 设备/关联系统 ID 使用 `-` 拼接；桥接只接受拆分后恰好三段的模拟 ID，`ExternalAPI::clearEvent()` 只找前两个 `-`，帧号用 `std::atoi()`，空串或非法串可变为 0 | 设备名或字段含 `-` 时解析歧义；非法帧号可能清除错误目标或静默未命中 | 待产品确认允许字符集；采用结构化参数或长度编码，并用可报告失败的严格整数解析 | 已记录于需求、概要、详细设计；未实施 |
| R-002 | 中 | 纯系统事件 ID 是单段 `eventName`，与 `event_types.h` 的统一三段式旧注释不一致 | 调用方可能按错误格式生成、显示或清除 ID | 选择并固化统一标识契约；决定保留单段时同步修正源码注释和外部协议 | 已记录；待产品确认，未实施 |
| R-003 | 高 | `EventManager::findEvent()` 在锁内取得内部元素地址，但返回前已解锁；删除、容器析构或无同步并发访问可使指针无效/不安全 | 调用方可能读悬空对象或产生数据竞争 | 改为按值/`optional` 快照返回，或提供受控锁内访问接口；废弃裸内部指针契约 | 已记录；未实施 |
| R-004 | 高 | 事件产生/清除持有非递归事件锁同步通知；默认同线程 Qt 信号立即刷新并再次查询同一管理器 | 未屏蔽产生和所有命中清除可死锁；产生日志或清除日志/删除可能无法到达 | 缩小锁范围，在提交内部状态后解锁再通知；定义回调重入策略并增加默认 UI 集成测试 | 已记录并在时序图标出停止点；未实施 |
| R-005 | 高 | 通知与 fallback 是进程全局单值回调；后初始化桥接覆盖前者，setter/read 无锁，lambda 捕获 `this`，析构不解绑 | 多桥接仅最后一个有效；并发读写存在数据竞争；桥接销毁后可能调用悬空对象 | 引入带令牌的订阅/退订和线程安全快照调用；用 QObject 寿命感知连接或弱引用；销毁时解除 | 已记录；未实施 |
| R-006 | 高 | `LinkageEngine` 的动作表、显示名、事件配置、等级默认、禁用集合和 fallback 没有互斥保护 | UI 配置与事件执行并发时可能数据竞争、迭代失效或读到不一致配置 | 明确单线程约束或增加读写锁；执行前复制不可变快照，避免持锁调用外部 callback | 已记录；未实施 |
| R-007 | 高 | 私有线程池只把并发数限制为 4，提交队列无上限、无背压/取消；`clearAll()` 先 `waitForDone()` 再清配置 | 慢/阻塞动作持续进入会积压内存和陈旧控制；动作不返回时清理可无限等待 | 增加有界队列、超时、背压和过期策略；将关闭流程与配置清理分离并提供可观测结果 | 已记录；未实施 |
| R-008 | 高 | `EventMgrModule::init()` 无锁且只检查 `api_`；并发首次调用可能重复分配/覆盖；各 `instance()`/`api()` 在未初始化时直接解引用空指针；没有 shutdown，动态对象不释放 | 初始化竞态、空指针崩溃、资源和线程池无法有序关闭 | 使用线程安全一次性初始化和显式错误返回；增加逆序 shutdown、回调解绑及线程池排空策略 | 已记录；未实施 |
| R-009 | 中 | 通知 JSON 通过字符串流手工拼接，字符串字段没有 JSON 转义 | 引号、反斜杠或控制字符可生成非法或语义被改变的消息 | 使用明确版本的 JSON 库或集中转义器，并为特殊字符和编码增加测试 | 已记录；未实施；引入库时须记录版本与编译方式 |
| R-010 | 高 | 当前降级只改变入表时的 `effectiveLevel` 和展示；联动始终依据 `originalLevel` | 与原始“降级后解除管控”语义冲突，操作人员可能误判控制效果 | 产品先确认安全语义；界面明确区分显示降级和联动抑制，若需解除管控应设计独立、可审计授权 | 已记录；待产品确认，未实施 |
| R-011 | 中 | 活跃事件加入后修改降级配置不会重算该事件存储的 `effectiveLevel`；UI 可显示“已降级”但颜色仍为旧等级 | 同一行的配置状态与显示等级可能不一致 | 产品确认配置是“仅后续事件生效”还是应更新活跃快照；若更新需定义通知和并发顺序 | 已记录；待产品确认，未实施 |
| R-012 | 高 | `BackendBridge` 当前同进程直调单例；`SocketServer` 只是无回调时打印 JSON 的桩，没有客户端、协议、重连、鉴权和确认 | 原始前后端分离目标无法验收，不能用于可靠跨进程部署 | 产品确认是否仍要求分离；若要求，单独设计协议、版本、状态同步、错误恢复和安全边界 | 已记录；待产品确认，未实施 |
| R-013 | 中 | 活跃事件、降级、屏蔽、联动配置/禁用均仅存内存，`LogWriter` 只输出标准输出 | 重启丢失配置与状态，无法提供历史审计或恢复 | 确认恢复点和审计保留要求；采用事务持久化、版本迁移和启动恢复策略 | 已记录；待产品确认，未实施 |
| R-014 | 中 | 动作禁用键直接拼为 `eventId + "\|" + actionName`，两部分没有转义 | 含 `\|` 的不同输入对可发生键碰撞，误禁用或误启用动作 | 使用结构化 pair 作为键并提供组合哈希，或禁止并校验分隔符 | 已记录；未实施 |
| R-015 | 中 | `ActionInfo`、`EventEntry`、`CatalogEntry`、`ActionEntry` 默认构造时部分 `bool`/`int` 标量值不确定；当前生产组装路径虽全部赋值，公开类型仍允许误用 | 新调用方默认构造后读取可产生未定义/不确定行为 | 在成员声明处默认初始化全部标量，并增加默认构造单元测试 | 已记录；未实施 |
| R-016 | 低 | 24 个可构造类型中仅 6 个构造和析构均显式，7 个只有显式构造，11 个均隐式 | 不满足项目“显式声明构造函数和析构函数”规则，也使所有权意图不够清晰 | 按详细设计第 13 节逐类补齐；静态工具类显式禁止实例化，QObject 类声明合适的虚析构覆盖 | 已完成合规审计；代码未实施 |
| R-017 | 高 | `AlarmCatalog`、`SocketServer`、`LogWriter`、`CmdSender`、`BuzzerControl` 均为静态示例或输出/延时桩 | 不能提供真实目录、通信、持久化、设备控制、确认、重试和故障恢复 | 在生产接入前定义适配接口、错误模型和验收测试；逐个替换桩并保留离线测试替身 | 已明确桩边界；未实施 |
| R-018 | 高 | `std::function` 回调允许抛出异常，但 `EventManager` 通知回调、`LinkageEngine` 同步 fallback 和异步 `ActionTask::run()` 均没有 `catch` 边界；证据见[详细设计公共回调契约](./2026-07-21-detailed-design.md#public-type-index)、[`EventManager::notifyFrontend()`](../../../backend/event_manager.cpp)和[`ActionTask`/fallback 实现](../../../backend/linkage_engine.cpp) | 同步通知或 fallback 抛出时会向事件调用方展开栈，`QMutexLocker` 会按 RAII 解锁，但事件链可能停在已入表、已改为 `Cleared` 或尚未写日志/删除的部分完成状态；异步 `QRunnable` 回调异常若逃出 `run()`，可能依运行时行为终止进程，且没有动作失败结果 | 为回调规定明确的 `noexcept` 契约，或在同步与异步边界分别捕获、记录并执行经产品确认的继续/回滚/失败策略；增加通知、fallback 和动作三类抛异常状态测试 | 已补充风险与源码/详细设计证据；未实施 |
| R-019 | 中 | 事件时间戳使用返回共享静态状态的 `std::localtime()`；`EventManager` 互斥锁只串行本模块的事件加入，不能保护进程内其他线程对 `localtime()` 的调用；证据见[详细设计并发矩阵](./2026-07-21-detailed-design.md#concurrency-matrix)和[`processAddEvent()`](../../../backend/event_manager.cpp) | 其他线程同时转换本地时间时可能发生数据竞争或读到被覆盖/损坏的时间字段，导致事件时间戳错误 | 封装线程安全时间转换层，在 POSIX 使用 `localtime_r()`、Windows 使用 `localtime_s()`，并用平台条件编译和并发测试验证 | 已补充风险与源码/详细设计证据；未实施 |

### 5.1 风险治理台账

优先级定义：P0 是进入下一轮默认路径/安全验证前的阻断项；P1 是生产集成或对外接口冻结前的门禁项；P2 是排入健壮性迭代的非阻断项；P3 是工程规则合规清理。所有里程碑日期当前均未排期，写作“日期待指定”；产品语义未决定的项目同时标记“待产品确认”。建议所有者是后续分工建议，不表示该角色已经接受任务。

| 编号 | 整改优先级 | 直接证据 | 建议所有者/决定所有者 | 目标里程碑与当前状态 | 关闭/接受条件 |
|---|---|---|---|---|---|
| R-001 | P1 | [详细设计标识规则](./2026-07-21-detailed-design.md#id-format)；[`ExternalAPI::clearEvent()`](../../../backend/external_api.cpp)；[`BackendBridge::triggerAlarm()`](../../../frontend/backend_bridge.cpp) | 后端 API 所有者；产品负责人决定允许字符和兼容策略 | M1 对外接口冻结前，日期待指定；待产品确认，未实施 | 形成版本化 ID 契约；非法帧号和分隔符输入可报告失败；设备/字段含边界字符的产生、模拟、清除测试通过 |
| R-002 | P2 | [详细设计 `EventId`](./2026-07-21-detailed-design.md#type-event-id)；[`event_types.h`](../../../backend/event_types.h)；[`ExternalAPI::triggerSystemEvent()`](../../../backend/external_api.cpp) | 产品负责人、系统架构所有者 | M2 标识一致性迭代，日期待指定；待产品确认，未实施 | 产品签署纯系统 ID 格式；源码注释、需求、协议示例和产生/清除测试全部一致 |
| R-003 | P1 | [详细设计 `EventManager`](./2026-07-21-detailed-design.md#module-event-manager)；[`EventManager::findEvent()`](../../../backend/event_manager.cpp) | 后端核心所有者 | M1 公共 API 冻结前，日期待指定；未实施 | 公共 API 不再返回无保护内部指针，或提供可证明安全的受控访问；删除/并发读取测试通过 |
| R-004 | P0 | [产生算法](./2026-07-21-detailed-design.md#add-flow)、[清除算法](./2026-07-21-detailed-design.md#clear-flow)；[`EventManager`](../../../backend/event_manager.cpp)；[`EventMgrWidget`](../../../frontend/eventmgr_widget.cpp) | 后端核心和 Qt 前端所有者 | M0 默认 UI 安全门禁，日期待指定；未实施 | 默认同线程 UI 中未屏蔽产生和命中清除均返回；日志/删除到达；重入及跨线程集成测试无死锁 |
| R-005 | P0 | [所有权矩阵](./2026-07-21-detailed-design.md#ownership-lifetime)；[`BackendBridge::initialize()`/析构](../../../frontend/backend_bridge.cpp)；[`setNotifyCallback`](../../../backend/event_manager.h) | 系统架构和 Qt 前端所有者 | M0 回调寿命门禁，日期待指定；未实施 | 多桥接订阅策略有明确契约；并发注册/通知安全；桥接销毁后不再回调；覆盖接管、退订和销毁测试 |
| R-006 | P0 | [并发矩阵](./2026-07-21-detailed-design.md#concurrency-matrix)；[`LinkageEngine`](../../../backend/linkage_engine.cpp) | 联动引擎所有者 | M0 并发安全门禁，日期待指定；未实施 | 配置读写采用明确单线程契约或同步/快照；事件执行与 UI 配置并发测试无数据竞争或迭代失效 |
| R-007 | P1 | [联动引擎错误边界](./2026-07-21-detailed-design.md#error-boundaries)；[`executeNames()`/`clearAll()`](../../../backend/linkage_engine.cpp) | 联动引擎和平台可靠性所有者 | M1 生产负载验证前，日期待指定；未实施 | 队列有上限、背压/过期和关闭时限；可观测拒绝/失败；慢动作压力及永不返回动作的关闭测试满足批准阈值 |
| R-008 | P1 | [初始化调用关系](./2026-07-21-detailed-design.md#init-flow)；[`EventMgrModule::init()`](../../../backend/event_mgr_module.cpp) | 系统架构和后端核心所有者 | M1 生命周期集成前，日期待指定；未实施 | 并发初始化只产生一组对象；初始化前访问可报告错误；shutdown 逆序解绑、排空并释放；生命周期测试通过 |
| R-009 | P2 | [通知 JSON](./2026-07-21-detailed-design.md#notification-json)；[`EventManager::notifyFrontend()`](../../../backend/event_manager.cpp) | 后端通知所有者、安全评审角色 | M2 输入健壮性迭代，日期待指定；未实施 | 特殊字符、控制字符和编码样例可被标准 JSON 解析器往返；若引库，版本与编译方式已记录 |
| R-010 | P0 | [联动解析](./2026-07-21-detailed-design.md#linkage-resolution)；[原始需求差异](./2026-07-21-software-requirements-baseline.md#8-原始需求差异)；[`LinkageEngine`](../../../backend/linkage_engine.cpp) | 产品负责人和功能安全决定者；后端/前端所有者实施 | M0 安全语义确认，日期待指定；待产品确认，未实施 | 产品书面确认“显示降级是否解除联动”；需求/UI 文案/联动测试与决定一致，并有可审计授权和回退方案 |
| R-011 | P2 | [`ConfigManager`](../../../backend/config_manager.cpp)；[`EventManager::processAddEvent()`](../../../backend/event_manager.cpp)；[`EventListWidget`](../../../frontend/event_list_widget.cpp) | 产品负责人决定时效；后端和前端所有者实施 | M2 配置一致性迭代，日期待指定；待产品确认，未实施 | 明确仅后续生效或重算活跃事件；配置状态、文字/颜色、通知和并发测试一致 |
| R-012 | P1 | [概要设计部署模式](./2026-07-21-high-level-design.md#6-部署与通信模式)；[`BackendBridge`](../../../frontend/backend_bridge.cpp)；[`SocketServer` 桩](../../../backend/stubs/socket_server.cpp) | 产品负责人决定交付范围；系统架构/通信所有者实施 | M1 部署架构冻结前，日期待指定；待产品确认，未实施 | 若取消分离，需求和文档明确关闭；若保留，协议版本、鉴权、重连、确认、状态同步及分进程验收全部通过 |
| R-013 | P2 | [所有权矩阵](./2026-07-21-detailed-design.md#ownership-lifetime)；[`EventManager`](../../../backend/event_manager.h)、[`ConfigManager`](../../../backend/config_manager.h)、[`LinkageEngine`](../../../backend/linkage_engine.h)、[`LogWriter` 桩](../../../backend/stubs/log_writer.cpp) | 产品负责人决定保留/恢复；平台数据所有者实施 | M2 状态恢复迭代，日期待指定；待产品确认，未实施 | 明确保留期、恢复点和迁移策略；重启恢复、故障一致性、历史检索和审计验收通过，或产品明确接受易失状态 |
| R-014 | P2 | [详细设计 `LinkageEngine`](./2026-07-21-detailed-design.md#module-linkage-engine)；[`makeDisableKey()`](../../../backend/linkage_engine.cpp) | 联动引擎所有者 | M2 键健壮性迭代，日期待指定；未实施 | 键改为结构化 pair 或入口严格拒绝 `\|`；碰撞构造测试证明不同输入不会互相禁用 |
| R-015 | P2 | [ActionInfo](./2026-07-21-detailed-design.md#type-action-info)、[EventEntry](./2026-07-21-detailed-design.md#type-event-entry)、[CatalogEntry](./2026-07-21-detailed-design.md#type-catalog-entry)、[ActionEntry](./2026-07-21-detailed-design.md#type-action-entry) | 公共 DTO/API 所有者 | M2 DTO 健壮性迭代，日期待指定；未实施 | 所有标量声明即初始化；默认构造和序列化/桥接测试可确定性读取全部字段 |
| R-016 | P3 | [构造与析构合规审计](./2026-07-21-detailed-design.md#ctor-dtor-audit)；相关声明见[`backend/`](../../../backend/event_mgr_module.h)和[`frontend/`](../../../frontend/event_list_widget.h) | 系统架构规则维护者、各组件所有者 | M3 工程合规清理，日期待指定；未实施 | 24 个可构造类型逐项复核；构造/析构显式声明或经批准的规则例外均有记录；编译和生命周期测试通过 |
| R-017 | P1 | [外部桩详细设计](./2026-07-21-detailed-design.md#stub-modules)；[`backend/stubs/`](../../../backend/stubs/alarm_catalog.cpp)；[`ActionRegistry`](../../../backend/action_registry.cpp) | 产品负责人决定生产边界；设备/通信/日志集成所有者实施 | M1 生产集成门禁，日期待指定；待产品确认具体适配范围，未实施 | 每个交付范围内桩由生产适配器替换并具错误、超时、重试/确认测试；未替换项明确排除出生产声明 |
| R-018 | P0 | [公共回调契约](./2026-07-21-detailed-design.md#public-type-index)；[`EventManager::notifyFrontend()`](../../../backend/event_manager.cpp)；[`ActionTask`/fallback](../../../backend/linkage_engine.cpp) | 系统架构和后端可靠性所有者 | M0 异常边界门禁，日期待指定；未实施 | 三类回调均有 `noexcept` 或捕获/记录/状态策略；抛异常测试验证锁释放、事件最终状态、日志和进程存活符合批准策略 |
| R-019 | P2 | [并发矩阵](./2026-07-21-detailed-design.md#concurrency-matrix)；[`processAddEvent()` 时间转换](../../../backend/event_manager.cpp) | 平台兼容和后端核心所有者 | M2 平台健壮性迭代，日期待指定；未实施 | 使用跨平台线程安全封装；并发时间转换测试无覆盖/损坏，目标工具链编译通过 |

## 6. 原始需求差异与待确认决定

完整当前行为和编号追踪见[当前需求基线第 8 节](./2026-07-21-software-requirements-baseline.md#8-原始需求差异)。本表只记录需要产品或架构确认的差异，不把建议写成实现事实。

| 原始要求/历史目标 | 当前事实 | 差异状态 | 待确认决定 |
|---|---|---|---|
| Qt 5.8.12 | 项目声明 Qt 5.15.10；本机验证为 5.15.3 | 已偏离 | 确认交付工具链锁定 5.15.10，或另行定义允许的兼容版本范围 |
| 后端不采用 Qt | 后端直接使用 Qt5Core；当前没有调用 QtConcurrent API | 已偏离 | 接受 Qt5Core 后端依赖，或批准替换同步与线程池设施的专项重构 |
| `protocolID + frameID` 标识设备状态帧 | 当前使用 `deviceName-frameID-alarmField`，纯系统事件仍是单段名称 | 已变更且格式不完全统一 | 确认允许字符、兼容迁移、纯系统格式和外部协议版本 |
| 降级用于忽视严重故障并解除管控 | 降级只影响加入时有效等级和显示，联动仍看原始等级 | 未满足原语义 | 决定继续“显示降级但保持联动”，还是新增独立且受控的联动抑制能力 |
| 单线程/串行处理 | 事件与配置使用锁，动作进入最多 4 线程的私有线程池 | 已变更 | 确认动作顺序、并发、超时、背压和失败处理要求 |
| 前后端可分离通信 | 当前只有同进程桥接；Socket 是打印桩 | 未实现 | 确认分离模式是否为交付要求及其可靠性、安全、协议范围 |
| 事件记录用于问题排查 | 当前日志只写标准输出，日志控件显示活跃事件而非历史记录 | 仅桩/部分实现 | 确认持久化介质、字段、保留期、检索和审计不可抵赖要求 |
| 屏蔽控制界面展示 | 产生通知和拉取视图分别过滤，清除通知不检查屏蔽；事件处理、日志和联动不停止 | 部分满足且存在通知缺陷 | 确认屏蔽仅影响可见性，还是还应影响通知、日志或联动；分别定义产生/清除语义 |
| 目标平台为飞腾 FT/2000 + 银河麒麟 V10 | 本轮只有 x86_64 Ubuntu 本机编译证据 | 未验证 | 安排目标机编译、运行、Qt 插件/ABI、性能和真实适配器验收 |

在产品决定形成前，当前基线继续如实描述代码现状。形成决定后，应先更新需求基线，再同步概要设计、详细设计、图表、讨论记录、README 和相应测试方案。

## 7. 2026-07-21 验证环境与结果

### 7.1 工具链记录

以下是 2026-07-21（Asia/Shanghai）本机观察值；OS 发行版命令是在 2026-07-22 质量复审时对当前验证主机补录。原始 2026-07-21 构建未保存 OS 命令输出，不能把补录结果当作不可变的当日原始日志。各工具命令和构建命令当时观察到退出码 0，但完整 stdout/stderr 原始日志、产物校验和及环境包清单没有留存，这是本记录的证据限制。

| 证据项/命令 | 观察输出 | 退出状态/证据限制 |
|---|---|---|
| `qmake -v` | `QMake version 3.1`；`Using Qt version 5.15.3 in /usr/lib/x86_64-linux-gnu` | 0；仅保留摘要 |
| `g++ --version \| head -1` | `g++ (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0` | 0；仅保留摘要 |
| `pkg-config --modversion Qt5Core Qt5Concurrent` | 两行均为 `5.15.3` | 0；仅保留摘要 |
| `sed -n '1,8p' /etc/os-release`（2026-07-22 补录） | `PRETTY_NAME="Ubuntu 22.04.5 LTS"`、`VERSION_ID="22.04"`、`VERSION_CODENAME=jammy` | 0；不是 2026-07-21 原始日志 |
| `pwd` | `/home/ff/EventMgr/.worktrees/documentation-baseline` | 0；隔离 worktree 工作目录 |
| `git branch --show-current` | `docs/documentation-baseline` | 0 |
| 源码审计基线 | `9a24bda74cde1b41ded65d858e4f89d92162a9be` | 固定审计标识；不是目标平台认证 |

原始验证在上述隔离 worktree 中执行。`9a24bda` 之后到 durable reviewed candidate `ffd994cd47cef502563473691de4b584cdafbd29` 的提交均只修改文档，没有改变 `main.cpp`、`backend/` 或 `frontend/` 源码；因此后续文档提交不改变已审代码内容。原始验证时未保留独立的 `git rev-parse HEAD`、`pwd`、`git status --short` 原始日志文件；上述工作目录、分支和审计 SHA 来自仓库/工作流记录。未来复现必须现场输出这些命令，并要求 `git status --short` 为空，同时核对源码相对审计基线无差异：

```bash
eventmgr_root=$(git rev-parse --show-toplevel)
source_audit_commit=9a24bda74cde1b41ded65d858e4f89d92162a9be
cd "$eventmgr_root"
pwd
git branch --show-current
git rev-parse HEAD
git status --short
git diff --exit-code "$source_audit_commit" -- main.cpp backend frontend
```

上述 `git status --short` 应无输出，`git diff --exit-code` 应以 0 退出；否则必须记录差异并建立新的源码审计基线，不能沿用本文构建结论。项目声明版本仍为 Qt 5.15.10、C++11，完整前端工程声明 `core gui widgets concurrent`。本机版本与项目声明必须分开解释。

### 7.2 编译与演示验证

以下命令是对原验证步骤的可复现改写：从当前仓库动态解析根目录，每次用 `mktemp -d` 创建新的仓库外输出目录，不复用固定 `/tmp` 路径，也不删除任何宽泛目录。计划基线的后端宽依赖编译当时观察到退出码 0：

```bash
eventmgr_root=$(git rev-parse --show-toplevel)
backend_verify_dir=$(mktemp -d /tmp/eventmgr-backend-verify.XXXXXX)
cd "$eventmgr_root"
g++ -std=c++11 -I. -Ibackend -Ibackend/stubs \
    main.cpp backend/*.cpp backend/stubs/*.cpp \
    -o "$backend_verify_dir/eventmgr-wide" \
    $(pkg-config --cflags --libs Qt5Core Qt5Concurrent) \
    -fPIC -pthread
backend_wide_exit=$?
printf 'backend-wide-exit=%s\n' "$backend_wide_exit"
test "$backend_wide_exit" -eq 0
```

前端仓库外 qmake/make 编译当时观察到退出码 0，未启动 GUI：

```bash
eventmgr_root=$(git rev-parse --show-toplevel)
frontend_verify_dir=$(mktemp -d /tmp/eventmgr-frontend-verify.XXXXXX)
cd "$frontend_verify_dir"
qmake "$eventmgr_root/frontend/frontend.pro" && make -j4
frontend_build_exit=$?
printf 'frontend-build-exit=%s\n' "$frontend_build_exit"
test "$frontend_build_exit" -eq 0
```

Task 4 进一步收紧后端直接依赖，只链接 Qt5Core；编译和运行六组演示当时均观察到退出码 0：

```bash
eventmgr_root=$(git rev-parse --show-toplevel)
backend_verify_dir=$(mktemp -d /tmp/eventmgr-backend-core-verify.XXXXXX)
cd "$eventmgr_root"
g++ -std=c++11 -fPIC -no-pie -I backend -I backend/stubs \
    main.cpp backend/*.cpp backend/stubs/*.cpp \
    $(pkg-config --cflags --libs Qt5Core) \
    -o "$backend_verify_dir/eventmgr-core-only"
backend_core_exit=$?
printf 'backend-core-build-exit=%s\n' "$backend_core_exit"
test "$backend_core_exit" -eq 0
"$backend_verify_dir/eventmgr-core-only"
backend_demo_exit=$?
printf 'backend-demo-exit=%s\n' "$backend_demo_exit"
test "$backend_demo_exit" -eq 0
```

`-fPIC -no-pie` 是本机 Qt reduce-relocations 配置下的验证参数，不自动成为其他工具链要求。运行该 Task 4 产物观察到根 `main.cpp` 的六组演示均执行到结束，最终输出活跃事件数 7。该演示不创建 `BackendBridge`，通知走标准输出桩，因而没有覆盖默认 UI 同线程死锁；场景 6 依靠固定休眠观察异步动作，也不是动作完成保证。

上述结果只证明审计快照在本机 Qt 5.15.3 / GCC 11.4.0 组合下可编译及后端演示可运行。飞腾 FT/2000、银河麒麟 V10、项目声明 Qt 5.15.10、GUI 启动行为和真实外部适配器均未验证。

### 7.3 图表验证

[架构与行为图](./2026-07-21-architecture-diagrams.md)已检查 Mermaid 代码块开闭结构、类/方法名称、调用方向、callback/fallback 条件和 UI 消费者边界。验证环境中 `mmdc` 不可用，因此没有完成渲染级检查；当前结论是“结构检查通过，渲染未验证”，不能声称图表已成功渲染。

## 8. Git/worktree 边界

- 原工作树 `master` 在本轮开始时已有用户改动：`build/test_eventmgr`、`frontend/frontend.pro.user` 和未跟踪目录 `build-frontend-unknown-Debug/`。这些内容保留原状，本轮未清理、覆盖或提交。
- 文档任务在 `docs/documentation-baseline` 分支和 `/home/ff/EventMgr/.worktrees/documentation-baseline` 隔离目录执行；构建验证输出位于仓库外 `/tmp`。后续复现按第 7.2 节使用 `mktemp -d` 创建新目录，不写入仓库构建目录，也不执行宽泛删除。
- 本轮没有修改业务代码，没有推送远程仓库。任何后续 `git push` 前必须先完成相关文档更新和全量验证。

## 9. 交付状态与更新规则

本表记录“撰写本文时”的状态；后续任务完成后由对应任务更新导航和最终验证记录，不应回写成未经验证的提前完成。

| 交付物 | 撰写时状态 | 提交/后续动作 |
|---|---|---|
| [文档导航](../../README.md) | 自动化工作流接受 | `27e162d45e4c503014f15494658fd34e33fdc48e`；非用户验收，Task 8 需更新最终状态 |
| [当前需求基线](./2026-07-21-software-requirements-baseline.md) | 自动化工作流接受 | `9a24bda74cde1b41ded65d858e4f89d92162a9be`；产品差异仍待确认 |
| [概要设计](./2026-07-21-high-level-design.md) | 自动化工作流接受 | `9d0bdd0f347df42dd6d2319a666a1dc59028814b`；非用户验收 |
| [详细设计](./2026-07-21-detailed-design.md) | 自动化工作流接受 | `913da30e76b9decd1bba83534ba2ffd849160075`；代码整改未实施 |
| [架构与行为图](./2026-07-21-architecture-diagrams.md) | 自动化工作流接受；渲染未验证 | `15483b44a91e4b42ab1be81cd0fb44301229a31d` |
| 本讨论与审计记录 | Task 6 v1.2 后续评审处置 | durable reviewed candidate 为 `ffd994cd47cef502563473691de4b584cdafbd29`；本次后续提交记录自动化工作流接受，处置提交 SHA 通过 `git log --follow`/tag/note 发现，不写入自身；19 项风险均未实施 |
| [项目 README](../../../README.md) | Task 7 待执行 | 更新当前术语、构建方式、业务语义和文档入口 |
| 全量一致性验证 | Task 8 待执行 | 检查格式、链接、术语、构建、演示和最终工作区边界 |

更新规则：代码行为、第三方库版本、编译方式、需求决定或风险处置发生变化时，必须在同一变更中更新受影响的需求、概要/详细设计、图表、讨论记录和 README；远程推送前完成文档和验证。若风险完成代码整改，应新增实现提交与测试证据，并把“未实施”改为可追溯的实际状态，不能只改措辞。

## 10. 后续优先级

本节严格采用第 5.1 节的整改优先级，不把风险严重度直接等同于排期。各项日期仍待指定，所有代码风险仍未实施：

1. P0 默认路径/安全门禁：`R-004` 默认 UI 死锁、`R-005` 回调所有权与寿命、`R-006` 联动容器并发、`R-010` 降级/联动安全语义、`R-018` 回调异常边界。其中降级/联动项必须先由产品/功能安全角色确认语义，其余由架构和代码所有者进入下一轮稳定化。
2. P1 生产集成/接口冻结门禁：`R-001` ID 解析、`R-003` 内部指针、`R-007` 无界队列/关闭等待、`R-008` 初始化与 shutdown、`R-012` Socket 分离范围、`R-017` 生产桩替换。高严重度但列入 P1 的原因是依赖接口/部署/生产范围确认，必须在相应门禁前关闭，不能因此带入生产。
3. P2 健壮性迭代：`R-002` 纯系统 ID/注释、`R-009` JSON 转义、`R-011` 活跃事件降级时效、`R-013` 持久化、`R-014` 禁用键碰撞、`R-015` DTO 标量、`R-019` 线程安全时间转换。涉及产品语义的项目先确认再实施。
4. P3 工程合规：`R-016` 显式构造/析构规则。按[构造与析构合规审计](./2026-07-21-detailed-design.md#ctor-dtor-audit)逐项关闭规则缺口。
5. 文档流程：本次新提交作为 `ffd994c` 之后的 Task 6 v1.2 自动化评审处置，不 amend candidate，也不为自写 SHA 再 amend；随后完成 Task 7 README 和 Task 8 全量验证。未经产品确认、代码实现和关闭条件验收，不把任何建议升级为“已修复/已实现”。

## 11. 2026-07-22 完成后状态与最终验证

本节是 2026-07-22（Asia/Shanghai）完成 Task 8 后补录的当前状态。第 9 节及第 10 节第 5 项保留“撰写本文时”的历史快照，不回写为提前完成；最终生效的文档/README 基线状态和阅读入口以[文档中心](../../README.md)为准。

### 11.1 版本与自动化评审记录

| 阶段 | 可核验修订 | 结果与权限边界 |
|---|---|---|
| Task 7 项目 README 最终评审版 | `bd453b74e286badf7c4ab4f69db593d348c67b69`（`bd453b7`） | [项目 README](../../../README.md)完成当前术语、依赖、构建、业务语义、风险提示和文档入口更新；自动化评审接受，不是用户验收或产品批准 |
| 当前文档导航生效 | `5fcef5cf5289621584a40dfec276300e585f6f41`（`5fcef5c`） | [文档中心](../../README.md)启用当前基线导航，并把历史资料和执行计划分开；自动化评审接受，不是用户验收或产品批准 |
| Task 8 全量一致性验证 | 被验证 HEAD `5fcef5cf5289621584a40dfec276300e585f6f41` | 实现代理执行自动化验证，规格与质量评审代理复核结果；这是文档工作流验证/评审，不代表用户、产品负责人、目标平台或生产环境批准 |
| 本记录 v1.3 | 本节的提交身份通过 `git log --follow -- docs/superpowers/specs/2026-07-21-documentation-discussion-record.md` 查询 | 新提交记录完成后状态，不 amend 既有审计提交，也不在正文自引用其尚未创建的提交 SHA |

### 11.2 Task 8 验证结果

| 检查项 | 2026-07-22 结果 | 口径与限制 |
|---|---|---|
| 工作树、差异与源码边界 | 隔离 worktree 在被验证 HEAD 上无未提交差异，`git diff --check` 通过；相对源码审计基线 `9a24bda74cde1b41ded65d858e4f89d92162a9be` 的 `main.cpp`、`backend/`、`frontend/` 无变化 | 文档分支只更新文档、README 和本地 worktree 忽略项；原工作树的用户改动保持原状 |
| 当前文档相对链接 | 8 个当前对外文档共解析 237 个相对 Markdown 链接，其中 139 个带锚点，文件目标和锚点缺失数均为 0 | 8 个文件是 `README.md`、`docs/README.md` 及 `docs/superpowers/specs/` 下 6 个 `2026-07-21-*.md` 规格文件；执行计划属于过程记录，不计入该 8 个文件。此前实现代理消息称“9 份”，评审按上述实际文件集合复核并以 8 份为准 |
| 需求追踪 | 需求基线定义的 71 个唯一 `CB-` 编号全部在详细设计追踪中出现，结果为 71/71 | 只证明文档编号映射完整，不替代逐项业务验收或代码测试 |
| Mermaid 结构 | 10 个 Mermaid 围栏块的开闭和结构检查通过 | 本机无 `mmdc`，未执行渲染验证，不能声称图表已成功渲染 |
| 后端最小依赖构建与演示 | 新建仓库外临时目录，仅链接 Qt5Core 编译成功；根 `main.cpp` 的六组演示运行并正常退出，最终活跃事件数为 7 | 使用本机 Qt 5.15.3/GCC 11.4.0；未创建 `BackendBridge`，不覆盖已知默认 GUI 死锁路径 |
| 前端构建 | 在新建的仓库外临时目录执行 qmake/make 编译成功 | 只验证编译，未启动 GUI |
| README 代码片段 | shell 片段通过语法检查，C++ 片段按 README 所述上下文完成语法核对 | 片段验证不等同于完整宿主集成或运行验收 |
| 原工作树与远程边界 | 原工作树中的 `build/test_eventmgr`、`frontend/frontend.pro.user` 和 `build-frontend-unknown-Debug/` 保持为用户改动；本轮未执行 `git push` | 推送前仍须按项目规则确认相关文档和验证状态 |

本轮没有在仓库中保留 Task 8 的完整原始 stdout/stderr 日志、临时构建产物校验和或环境包清单。第 7.1、7.2 节保留工具链、源码差异、后端和前端验证命令，可在相同前提下重新执行；摘要不能替代原始日志或目标平台认证证据。

### 11.3 剩余验证限制

- 当前编译和演示证据仅来自 Ubuntu 22.04 x86_64、Qt 5.15.3、GCC 11.4.0；飞腾 FT/2000、银河麒麟 V10 和项目声明的 Qt 5.15.10 均未验证。
- 已知默认 GUI 同线程通知死锁仍未整改，因此没有启动 GUI；这不是 GUI 运行验收通过。
- `AlarmCatalog`、`SocketServer`、`LogWriter`、`CmdSender` 和 `BuzzerControl` 等真实外部适配器未接入或未测试。
- Mermaid 仅完成 10 个代码块的结构检查，因 `mmdc` 不可用而未渲染。
- 第 5 节 19 项风险及第 5.1 节治理状态均保持“未实施”或“待产品确认”；文档完成不改变代码风险状态。

## 12. 2026-07-22 需求文档按代码对齐确认

### 12.1 用户确认

用户明确确认“以当前代码为准，更新所有需规文件”，并批准分层修订方案：当前需求基线完整对齐代码；历史需规保留正文并增加失效声明和差异摘要；原始需求和讨论记录保留用于回溯。

### 12.2 生效规则

- [当前需求基线](./2026-07-21-software-requirements-baseline.md)是唯一当前需求与验收基线。
- `doc/requirment.md` 是原始需求输入，不表示当前实现。
- 2026-06-26 v1.1 和 2026-07-06 v2.2 需规是历史归档，不得用于当前开发或验收。
- 当前 EventId 契约为：设备事件 `deviceName-frameID-alarmField`，关联设备系统事件 `deviceName-0-eventName`，纯系统事件 `eventName`。
- 本次只修改文档，不修改 EventId 代码、解析限制或其他软件行为。

### 12.3 实施依据

修订设计见[需求文档按当前代码对齐设计](./2026-07-22-requirements-code-alignment-design.md)，执行步骤见[需求文档按代码对齐计划](../plans/2026-07-22-requirements-code-alignment.md)。最终验证结果在本节后续小节补录。
