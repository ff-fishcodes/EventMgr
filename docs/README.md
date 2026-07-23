# EventMgr 文档中心

本目录按版本区分当前基线与历史资料。2026-07-23 当前基线已同步到事件联动配置中心源码 `89dad8b`，其中当前需求基线是唯一有效的需求与验收文档。原始需求和历史设计仅用于追溯，执行计划记录实施过程而不替代设计或验收。

## 当前基线

| 文档 | 状态 | 说明 |
|------|------|------|
| [当前需求基线](superpowers/specs/2026-07-21-software-requirements-baseline.md) | 当前实现 | 当前代码已实现的外部行为、业务规则、限制及需求差异 |
| [概要设计](superpowers/specs/2026-07-21-high-level-design.md) | 当前实现 | 系统上下文、分阶段联动、单锁快照、依赖和总体设计决策 |
| [详细设计](superpowers/specs/2026-07-21-detailed-design.md) | 当前实现 | DTO、接口、所有权、算法、并发和配置页状态机 |
| [架构与行为图](superpowers/specs/2026-07-21-architecture-diagrams.md) | 当前实现 | 类图、配置调用关系、两阶段执行和离开保护时序 |
| [讨论与审计记录](superpowers/specs/2026-07-21-documentation-discussion-record.md) | 当前实现 | 范围选择、方案比较、验证证据、风险和后续建议 |
| [文档基线设计方案](superpowers/specs/2026-07-21-documentation-baseline-design.md) | 已确认 | 本轮文档基线的范围、组织方式、事实提取和一致性规则 |
| [需求文档按当前代码对齐设计](superpowers/specs/2026-07-22-requirements-code-alignment-design.md) | 已确认 | 当前需规、历史需规、原始需求和讨论记录的分层修订规则 |
| [事件联动配置中心设计](superpowers/specs/2026-07-22-event-configuration-linkage-controls-design.md) | 已确认 | 全目录、分阶段有效动作、暂存应用与单锁快照的批准方案 |

以上当前交付文档共同构成当前基线；当前需求基线是唯一有效的需求与验收文档，设计方案是已确认的编制依据，不替代需求、概要设计或详细设计。

## 实施记录

| 文档 | 状态 | 说明 |
|------|------|------|
| [文档基线执行计划](superpowers/plans/2026-07-21-documentation-baseline.md) | 已执行（文档任务） | 本轮需求、设计、图表、讨论记录和导航完善任务的执行依据；不表示风险台账中的代码整改已经实施 |
| [需求文档按代码对齐计划](superpowers/plans/2026-07-22-requirements-code-alignment.md) | 已执行（文档任务） | 需求基线复核、历史状态声明、讨论记录、导航和一致性验证步骤 |
| [事件联动配置中心实施计划](superpowers/plans/2026-07-22-event-configuration-linkage-controls.md) | 实施依据 | 后端、前端、测试和 Task 8/Task 9 追溯；计划复选框保持原记录，本导航不据此宣称 Task 10 或远程集成完成 |

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
| 2026-06-26 | [历史软件需求规格（已失效）](superpowers/specs/2026-06-26-software-requirements-spec.md) |
| 2026-07-06 | [历史软件需求规格（已失效）](superpowers/specs/2026-07-06-software-requirements-spec.md) |
| 2026-07-06 | [软件设计说明](superpowers/specs/2026-07-06-software-design-spec.md) |

## 文档维护规则

- 当前实现事实优先从头文件、实现文件、Qt 工程文件和入口程序取证，并与当前提交历史交叉核验。
- 原始需求、当前实现、风险、建议和待决事项必须使用状态标识明确区分。
- 新的当前基线使用日期命名并更新本导航；既有版本保留为历史追溯资料，不原地改写其设计结论。
- 软件设计文档同时维护概要设计和详细设计；实现变化涉及类关系、调用关系或交互流程时，同步更新相应图表。
- 推送远程仓库前先完善相关文档。本轮实施过程可通过[文档基线执行计划](superpowers/plans/2026-07-21-documentation-baseline.md)追溯。

## 状态标识

| 标识 | 含义 |
|------|------|
| 已确认 | 范围或方案已完成确认，可作为本轮文档工作的依据 |
| 已完成并审查 | 文档内容已生成、核对并纳入当前生效基线 |
| 已执行（文档任务） | 执行计划所列的文档交付任务已实施，不代表其中记录的代码风险已整改 |
| 当前实现 | 能够从当前代码或构建配置中核验的事实 |
| 原始需求 | 需求来源中的目标描述，不等同于已经实现 |
| 风险 | 当前实现中需要明确告知的限制或不一致 |
| 建议 | 后续可能采取的改进方向，本轮不承诺实现 |
| 待决事项 | 尚未形成结论，需要后续确认 |
| 历史 | 仅用于追溯，不代表当前实现 |
