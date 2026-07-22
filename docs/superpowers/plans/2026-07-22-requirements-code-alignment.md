# Requirements Code Alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 以当前代码为唯一实现事实更新全部需求相关文档，同时保留原始需求、历史需规和讨论过程的版本追溯能力。

**Architecture:** 文档采用“一个当前基线、多个明确失效的历史版本”结构。当前基线逐项链接源码证据；历史正文不做无痕改写，只在文首增加统一状态声明和关键差异；讨论记录与导航同步记录本次确认和生效入口。

**Tech Stack:** Markdown、Git、Bash、ripgrep；事实来源为 C++11/Qt 5 项目源码及 qmake 工程文件。

---

## File Structure

| 文件 | 责任 | 本计划动作 |
|---|---|---|
| `docs/superpowers/specs/2026-07-21-software-requirements-baseline.md` | 唯一当前需求与验收基线 | 增加当前状态、确认日期、文档分层和 EventId 解析约束 |
| `docs/superpowers/specs/2026-07-21-detailed-design.md` | 当前详细设计与需求追踪矩阵 | 把新增文档权威和 EventId 解析要求纳入追踪，不改变软件设计 |
| `doc/requirment.md` | 用户最初提供的原始需求输入 | 文首增加原始输入声明和当前基线链接，正文不改 |
| `docs/superpowers/specs/2026-06-26-software-requirements-spec.md` | v1.1 历史需规 | 文首增加已失效声明和当前代码关键差异 |
| `docs/superpowers/specs/2026-07-06-software-requirements-spec.md` | v2.2 历史需规 | 文首增加已失效声明和当前代码关键差异 |
| `docs/superpowers/specs/2026-06-17-requirement-discussion-record.md` | 早期需求决策过程 | 文末追加后续基线状态，不修改原讨论 |
| `docs/superpowers/specs/2026-07-21-documentation-discussion-record.md` | 当前文档讨论、风险与验证记录 | 追加本次用户确认、修订方案和验证结果 |
| `README.md` | 项目首屏说明 | 明确当前需规入口及历史文档不可用于验收 |
| `docs/README.md` | 文档状态和导航 | 纳入本次设计/计划，明确唯一有效需规与历史状态 |

### Task 1: Strengthen The Current Requirements Baseline

**Files:**
- Modify: `docs/superpowers/specs/2026-07-21-software-requirements-baseline.md`
- Modify: `docs/superpowers/specs/2026-07-21-detailed-design.md`
- Reference: `backend/external_api.cpp`
- Reference: `backend/event_manager.cpp`
- Reference: `backend/event_types.h`
- Reference: `frontend/backend_bridge.cpp`

- [ ] **Step 1: Capture the current code evidence for EventId**

Run:

```bash
rg -n 'using EventId|event\.id =|makeEventId|clearEvent\(|std::atoi' \
  backend/event_types.h backend/external_api.cpp backend/event_manager.cpp
```

Expected: output shows `EventId` as `std::string`, device ID construction, pure-system assignment to `eventName`, associated-system three-part construction, and full-ID clear parsing.

- [ ] **Step 2: Add a version and status block to the current baseline**

Immediately after the H1, add:

```markdown
> 文档状态：当前有效、唯一需求与验收基线  
> 代码复核日期：2026-07-22  
> 适用范围：当前仓库代码；历史需求和历史需规仅用于追溯
```

Change the opening paragraph date from “2026-07-21 工作树” to “截至 2026-07-22 的当前代码”，and link the alignment design:

```markdown
本次文档分层和历史保留规则见[需求文档按当前代码对齐设计](./2026-07-22-requirements-code-alignment-design.md)。
```

- [ ] **Step 3: Add the document authority requirement**

Append this item to section 1:

```markdown
- **CB-DOC-004**：本文件是当前唯一有效的需求与验收基线。`doc/requirment.md`、2026-06-26 和 2026-07-06 需求规格保留为原始输入或历史版本；其中与当前代码不一致的接口、依赖、标识和行为不得用于当前开发或验收。证据：`doc/requirment.md`、`docs/superpowers/specs/2026-06-26-software-requirements-spec.md`、`docs/superpowers/specs/2026-07-06-software-requirements-spec.md`、`docs/superpowers/specs/2026-07-22-requirements-code-alignment-design.md`。
```

- [ ] **Step 4: Make the EventId parsing limitations normative**

Append this item to section 4:

```markdown
- **CB-ID-006**：当前 ID 拼接不转义 `-`。`ExternalAPI::clearEvent(eventId)` 按前两个 `-` 切分，并用 `std::atoi()` 解析中段；因此字段含 `-` 或帧号文本非法时存在解析歧义，调用方不得把这些输入视为已支持。证据：`backend/external_api.cpp`、`frontend/backend_bridge.cpp`。
```

Update the section 9 acceptance matrix so `CB-DOC-004` is covered by the documentation traceability row and `CB-ID-006` is covered by the EventId row. Do not renumber existing requirements.

In detailed-design section 15, make these exact tracking updates:

```markdown
| [证据边界](#doc-metadata) | `CB-DOC-003`、`CB-DOC-004` |
| [设备/系统标识与解析](#id-format) | `CB-ID-001`、`CB-ID-002`、`CB-ID-006`、`CB-LIM-001`、`CB-LIM-002` |
```

- [ ] **Step 5: Verify current-baseline terminology**

Run:

```bash
rg -n '^\- \*\*CB-.*(EventId.*protocolID|事件名:下位机|纯系统.*系统-0-)' \
  docs/superpowers/specs/2026-07-21-software-requirements-baseline.md
rg -n 'CB-DOC-004|CB-ID-006|deviceName-frameID-alarmField|deviceName-0-eventName|纯系统事件 ID 当前等于' \
  docs/superpowers/specs/2026-07-21-software-requirements-baseline.md
```

Expected: the first command has no output; the second command finds the new authority and parsing requirements plus all three current EventId rules.

- [ ] **Step 6: Commit the current baseline update**

```bash
git add docs/superpowers/specs/2026-07-21-software-requirements-baseline.md \
  docs/superpowers/specs/2026-07-21-detailed-design.md
git commit -m "docs: align current requirements baseline with code"
```

### Task 2: Mark Original And Historical Requirements Correctly

**Files:**
- Modify: `doc/requirment.md`
- Modify: `docs/superpowers/specs/2026-06-26-software-requirements-spec.md`
- Modify: `docs/superpowers/specs/2026-07-06-software-requirements-spec.md`

- [ ] **Step 1: Add the original-input notice without rewriting the source text**

Insert before the existing first line of `doc/requirment.md`:

```markdown
> **文档属性：原始需求输入，非当前需求或验收基线。**
>
> 本文保留用户最初提供的目标、术语和约束，用于需求演进回溯；其中 `protocolID`、Qt 5.8.12、后端不依赖 Qt、前后端 Socket 分离及降级解除管控等内容与当前代码存在差异。当前开发和验收统一以[当前需求基线](../docs/superpowers/specs/2026-07-21-software-requirements-baseline.md)为准。

```

Do not change any existing full-width Markdown heading or original sentence below the notice.

- [ ] **Step 2: Add the v1.1 historical-version notice**

Insert after the H1 in `2026-06-26-software-requirements-spec.md`:

```markdown
> **版本状态：历史归档，已被[当前需求基线](./2026-07-21-software-requirements-baseline.md)取代。**  
> 本文保留 v1.1 的原始评审内容，仅用于版本演进回溯，不得作为当前开发或验收依据。当前代码已由 `protocolID` 改为 `deviceName`；设备事件 ID 为 `deviceName-frameID-alarmField`，关联设备系统事件 ID 为 `deviceName-0-eventName`，纯系统事件 ID 为 `eventName`。当前后端直接依赖 Qt5Core，前后端通过同进程桥接运行，其他行为与限制以当前基线为准。
```

Keep the original `版本：v1.1` and `状态：已评审` lines as historical metadata.

- [ ] **Step 3: Add the v2.2 historical-version notice**

Insert after the H1 in `2026-07-06-software-requirements-spec.md`:

```markdown
> **版本状态：历史归档，已被[当前需求基线](./2026-07-21-software-requirements-baseline.md)取代。**  
> 本文保留 v2.2 的原始内容，仅用于版本演进回溯，不得作为当前开发或验收依据。当前代码的设备事件 ID 为 `deviceName-frameID-alarmField`，关联设备系统事件 ID 为 `deviceName-0-eventName`，纯系统事件 ID 为 `eventName`；当前仅实现同进程桥接，Socket 分离仍是桩边界；后端直接依赖 Qt5Core。其他行为与限制以当前基线为准。
```

Keep the original document number, version, date, change description, and body unchanged.

- [ ] **Step 4: Prove that only the headers changed**

Run:

```bash
git diff --word-diff=porcelain -- \
  doc/requirment.md \
  docs/superpowers/specs/2026-06-26-software-requirements-spec.md \
  docs/superpowers/specs/2026-07-06-software-requirements-spec.md
```

Expected: additions occur only before the original raw requirement heading or immediately below each historical H1; no original line is deleted.

- [ ] **Step 5: Verify every historical requirement points to the current baseline**

Run:

```bash
for file in \
  doc/requirment.md \
  docs/superpowers/specs/2026-06-26-software-requirements-spec.md \
  docs/superpowers/specs/2026-07-06-software-requirements-spec.md; do
  rg -q '2026-07-21-software-requirements-baseline\.md' "$file" || exit 1
done
```

Expected: exit status 0 with no output.

- [ ] **Step 6: Commit the historical status notices**

```bash
git add doc/requirment.md \
  docs/superpowers/specs/2026-06-26-software-requirements-spec.md \
  docs/superpowers/specs/2026-07-06-software-requirements-spec.md
git commit -m "docs: classify historical requirements versions"
```

### Task 3: Record The Decision And Update Navigation

**Files:**
- Modify: `docs/superpowers/specs/2026-06-17-requirement-discussion-record.md`
- Modify: `docs/superpowers/specs/2026-07-21-documentation-discussion-record.md`
- Modify: `README.md`
- Modify: `docs/README.md`

- [ ] **Step 1: Append the later-status note to the 2026-06-17 discussion record**

Append this section without changing the prior numbered discussion:

```markdown

---

## 20. 2026-07-22 后续状态

用户确认需求文档以当前代码为准，并采用分层修订：当前需求基线作为唯一有效需求和验收依据；本文及早期需求、设计继续作为历史讨论记录，不回写早期结论。当前生效入口为[当前需求基线](./2026-07-21-software-requirements-baseline.md)，修订范围与保留规则见[需求文档按当前代码对齐设计](./2026-07-22-requirements-code-alignment-design.md)。
```

- [ ] **Step 2: Append the current decision record**

Append this section to `2026-07-21-documentation-discussion-record.md`:

```markdown

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
```

- [ ] **Step 3: Update the project README requirement entry**

In the README documentation section, ensure the current requirement link contains this qualification:

```markdown
- [当前需求基线](docs/superpowers/specs/2026-07-21-software-requirements-baseline.md)：唯一当前需求与验收依据；原始需求和旧版需规仅用于追溯。
```

Do not duplicate the existing EventId table; it already states the three current formats.

- [ ] **Step 4: Update the document center**

In `docs/README.md`:

1. Change “五份 2026-07-21 基线交付文档” to wording that does not encode a stale count.
2. State that the current requirements baseline is the only effective requirements/acceptance document.
3. Add the alignment design under current supporting documents:

```markdown
| [需求文档按当前代码对齐设计](superpowers/specs/2026-07-22-requirements-code-alignment-design.md) | 已确认 | 当前需规、历史需规、原始需求和讨论记录的分层修订规则 |
```

4. Add the implementation plan under implementation records:

```markdown
| [需求文档按代码对齐计划](superpowers/plans/2026-07-22-requirements-code-alignment.md) | 执行中 | 需求基线复核、历史状态声明、讨论记录、导航和一致性验证步骤 |
```

5. Change the two historical requirements labels to “历史软件需求规格（已失效）”.

- [ ] **Step 5: Check discussion and navigation links**

Run:

```bash
rg -n '唯一当前需求|历史归档|已失效|2026-07-22-requirements-code-alignment' \
  README.md docs/README.md \
  docs/superpowers/specs/2026-06-17-requirement-discussion-record.md \
  docs/superpowers/specs/2026-07-21-documentation-discussion-record.md
```

Expected: all four files describe the same authority rule and both discussion records link to the current baseline.

- [ ] **Step 6: Commit the decision and navigation update**

```bash
git add README.md docs/README.md \
  docs/superpowers/specs/2026-06-17-requirement-discussion-record.md \
  docs/superpowers/specs/2026-07-21-documentation-discussion-record.md
git commit -m "docs: record current requirements authority"
```

### Task 4: Run Full Documentation Consistency Verification

**Files:**
- Modify: `docs/superpowers/specs/2026-07-21-documentation-discussion-record.md`
- Modify: `docs/README.md`
- Verify: all files changed by Tasks 1-3

- [ ] **Step 1: Check formatting and the source-code boundary**

Run:

```bash
git diff --check HEAD~3..HEAD
git diff --name-only HEAD~3..HEAD | rg -v '^(README\.md|doc/|docs/)'
```

Expected: `git diff --check` exits 0; the second command prints nothing, proving no source file changed.

- [ ] **Step 2: Verify current EventId consistency**

Run:

```bash
rg -n 'deviceName-frameID-alarmField|deviceName-0-eventName|纯系统事件.*eventName' \
  README.md docs/README.md \
  docs/superpowers/specs/2026-07-21-software-requirements-baseline.md \
  docs/superpowers/specs/2026-07-21-high-level-design.md \
  docs/superpowers/specs/2026-07-21-detailed-design.md
```

Expected: each current document that defines EventId uses the same three formats; no current document defines `protocolID` or `事件名:下位机标识` as the current format.

- [ ] **Step 3: Verify requirements authority markers**

Run:

```bash
rg -q '原始需求输入，非当前需求或验收基线' doc/requirment.md
rg -q '版本状态：历史归档' docs/superpowers/specs/2026-06-26-software-requirements-spec.md
rg -q '版本状态：历史归档' docs/superpowers/specs/2026-07-06-software-requirements-spec.md
rg -q '唯一需求与验收基线' docs/superpowers/specs/2026-07-21-software-requirements-baseline.md
```

Expected: all commands exit 0.

- [ ] **Step 4: Validate relative Markdown links and explicit anchors**

Run:

```bash
node <<'NODE'
const fs = require('fs');
const path = require('path');

const files = [
  'README.md',
  'docs/README.md',
  'doc/requirment.md',
  'docs/superpowers/specs/2026-06-26-software-requirements-spec.md',
  'docs/superpowers/specs/2026-07-06-software-requirements-spec.md',
  ...fs.readdirSync('docs/superpowers/specs')
    .filter(name => /^2026-07-(21|22)-.*\.md$/.test(name))
    .map(name => `docs/superpowers/specs/${name}`),
];

function anchorsFor(file) {
  const text = fs.readFileSync(file, 'utf8');
  const anchors = new Set();
  const duplicates = [];
  for (const match of text.matchAll(/<a\s+id=["']([^"']+)["'][^>]*>/gi)) {
    if (anchors.has(match[1])) duplicates.push(`${file}#${match[1]}`);
    anchors.add(match[1]);
  }
  return {anchors, duplicates};
}

let links = 0;
let anchoredLinks = 0;
const errors = [];
const duplicateAnchors = [];
const anchorCache = new Map();

for (const file of [...new Set(files)].sort()) {
  const own = anchorsFor(file);
  duplicateAnchors.push(...own.duplicates);
  anchorCache.set(path.resolve(file), own.anchors);
  const text = fs.readFileSync(file, 'utf8');
  for (const match of text.matchAll(/\[[^\]]*\]\(([^)]+)\)/g)) {
    let target = match[1].trim().replace(/^<|>$/g, '');
    if (/^(https?:|mailto:)/.test(target)) continue;
    links += 1;
    const hashAt = target.indexOf('#');
    const rawPath = hashAt >= 0 ? target.slice(0, hashAt) : target;
    const anchor = hashAt >= 0 ? decodeURIComponent(target.slice(hashAt + 1)) : '';
    const resolved = path.resolve(path.dirname(file), rawPath || path.basename(file));
    if (!fs.existsSync(resolved)) {
      errors.push(`${file}: missing ${target}`);
      continue;
    }
    if (anchor) {
      anchoredLinks += 1;
      if (!anchorCache.has(resolved)) anchorCache.set(resolved, anchorsFor(resolved).anchors);
      if (!anchorCache.get(resolved).has(anchor)) errors.push(`${file}: missing anchor ${target}`);
    }
  }
}

console.log(`files=${new Set(files).size} links=${links} anchored=${anchoredLinks}`);
console.log(`missing=${errors.length} duplicate-explicit-anchors=${duplicateAnchors.length}`);
for (const error of [...errors, ...duplicateAnchors]) console.error(error);
if (errors.length || duplicateAnchors.length) process.exit(1);
NODE
```

Expected: exit status 0, `missing=0`, and `duplicate-explicit-anchors=0`. Record the actual file/link/anchor counts rather than copying earlier counts.

- [ ] **Step 5: Verify requirement identifier traceability after adding two IDs**

Run:

```bash
rg -o 'CB-[A-Z]+-[0-9]{3}' \
  docs/superpowers/specs/2026-07-21-software-requirements-baseline.md \
  | sort -u > /tmp/eventmgr-requirement-ids.txt
while read -r id; do
  rg -q "$id" docs/superpowers/specs/2026-07-21-detailed-design.md || {
    printf 'missing detailed-design trace: %s\n' "$id"
    exit 1
  }
done < /tmp/eventmgr-requirement-ids.txt
wc -l /tmp/eventmgr-requirement-ids.txt
```

Expected: exit status 0, no missing trace messages, and count `73`; the pre-change baseline contained 71 unique IDs and this plan adds exactly `CB-DOC-004` and `CB-ID-006`.

- [ ] **Step 6: Append actual verification evidence**

Under section 12 of `2026-07-21-documentation-discussion-record.md`, append `### 12.4 验证结果` with:

- verified commit or pre-final HEAD;
- changed-file list and confirmation that no source code changed;
- actual Markdown link and anchor counts;
- actual unique `CB-` count;
- EventId terminology scan result;
- preserved limitations: source comment mismatch, delimiter/`atoi` parsing risk, target platform not reverified, and no remote push.

Use only values returned by Steps 1-5. Do not reuse the older 71/71 or 248-link counts unless the new run independently returns them.

- [ ] **Step 7: Mark the plan status complete in navigation**

In `docs/README.md`, change only the new alignment plan row from `执行中` to `已执行（文档任务）` after Steps 1-6 pass.

- [ ] **Step 8: Commit the verification record**

```bash
git add docs/README.md \
  docs/superpowers/specs/2026-07-21-documentation-discussion-record.md
git commit -m "docs: verify requirements documentation alignment"
git status --short --branch
```

Expected: commit succeeds and the final status shows branch `docs/documentation-baseline` with no uncommitted files. Do not run `git push`.
