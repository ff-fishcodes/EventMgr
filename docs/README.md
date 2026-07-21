# EventMgr 文档中心

本目录按版本区分当前基线与历史资料。五份 2026-07-21 基线交付文档完成生成并通过核验后，方成为当前文档基线；在此之前，当前事实以代码和构建配置为准。原始需求、历史设计和执行计划仅用于追溯。

## 当前基线

| 文档 | 状态 | 说明 |
|------|------|------|
| [当前需求基线](superpowers/specs/2026-07-21-software-requirements-baseline.md) | 计划中 | 当前代码已实现的外部行为、业务规则、限制及需求差异 |
| [概要设计](superpowers/specs/2026-07-21-high-level-design.md) | 计划中 | 系统上下文、模块边界、数据流、依赖和总体设计决策 |
| [详细设计](superpowers/specs/2026-07-21-detailed-design.md) | 计划中 | 类型、类、接口、成员、生命周期、算法和并发边界 |
| [架构与行为图](superpowers/specs/2026-07-21-architecture-diagrams.md) | 计划中 | 类图、模块调用关系图和主要业务时序图 |
| [讨论与审计记录](superpowers/specs/2026-07-21-documentation-discussion-record.md) | 计划中 | 范围选择、方案比较、审计发现、风险和后续建议 |
| [文档基线设计方案](superpowers/specs/2026-07-21-documentation-baseline-design.md) | 已确认 | 本轮文档基线的范围、组织方式、事实提取和一致性规则 |

标记为“计划中”的五个链接是本轮文档刷新约定的交付路径，将由后续任务创建；在文件生成前出现链接未解析属于预期状态。

## 推荐阅读顺序

1. 阅读[文档基线设计方案](superpowers/specs/2026-07-21-documentation-baseline-design.md)，了解范围、事实基线与非目标。
2. 阅读[当前需求基线](superpowers/specs/2026-07-21-software-requirements-baseline.md)，确认系统已实现行为及其限制。
3. 依次阅读[概要设计](superpowers/specs/2026-07-21-high-level-design.md)和[详细设计](superpowers/specs/2026-07-21-detailed-design.md)，理解系统边界与模块实现。
4. 结合[架构与行为图](superpowers/specs/2026-07-21-architecture-diagrams.md)核对类关系、调用方向和业务时序。
5. 阅读[讨论与审计记录](superpowers/specs/2026-07-21-documentation-discussion-record.md)，追溯方案选择、风险与待决事项。

## 历史文档

以下文件均为历史追溯资料，用于还原需求和设计演进，不代表当前实现；引用其中结论时必须与当前代码和 2026-07-21 基线复核。

| 日期 | 文档 |
|------|------|
| 2026-06-17 | [EventMgr 设计方案](superpowers/specs/2026-06-17-event-mgr-design.md) |
| 2026-06-17 | [需求讨论记录](superpowers/specs/2026-06-17-requirement-discussion-record.md) |
| 2026-06-17 | [软件设计说明](superpowers/specs/2026-06-17-software-design-spec.md) |
| 2026-06-25 | [架构图](superpowers/specs/2026-06-25-architecture-diagrams.md) |
| 2026-06-26 | [ActionRegistry 设计](superpowers/specs/2026-06-26-action-registry-design.md) |
| 2026-06-26 | [软件需求规格](superpowers/specs/2026-06-26-software-requirements-spec.md) |
| 2026-07-06 | [软件需求规格](superpowers/specs/2026-07-06-software-requirements-spec.md) |
| 2026-07-06 | [软件设计说明](superpowers/specs/2026-07-06-software-design-spec.md) |

## 文档维护规则

- 当前实现事实优先从头文件、实现文件、Qt 工程文件和入口程序取证，并与当前提交历史交叉核验。
- 原始需求、当前实现、风险、建议和待决事项必须使用状态标识明确区分。
- 新的当前基线使用日期命名并更新本导航；既有版本保留为历史追溯资料，不原地改写其设计结论。
- 软件设计文档同时维护概要设计和详细设计；实现变化涉及类关系、调用关系或交互流程时，同步更新相应图表。
- 推送远程仓库前先完善相关文档。实施步骤以[文档基线执行计划](superpowers/plans/2026-07-21-documentation-baseline.md)为参考。

## 状态标识

| 标识 | 含义 |
|------|------|
| 已确认 | 范围或方案已完成确认，可作为本轮文档工作的依据 |
| 当前实现 | 能够从当前代码或构建配置中核验的事实 |
| 原始需求 | 需求来源中的目标描述，不等同于已经实现 |
| 风险 | 当前实现中需要明确告知的限制或不一致 |
| 建议 | 后续可能采取的改进方向，本轮不承诺实现 |
| 待决事项 | 尚未形成结论，需要后续确认 |
| 计划中 | 已约定交付路径，但文件尚未由对应后续任务创建 |
| 历史 | 仅用于追溯，不代表当前实现 |
