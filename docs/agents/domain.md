# Domain Docs

## Before exploring

Read these when present:

- Root `CONTEXT.md`
- Relevant ADRs under `docs/adr/`

Missing files are not errors. Proceed silently; create domain docs lazily through `domain-modeling` when terminology or decisions are resolved.

## Layout

This is a single-context repository:

```text
/
├── CONTEXT.md
└── docs/adr/
```

## Vocabulary

Use terms defined in `CONTEXT.md` in issues, designs, tests, and code. Avoid synonyms explicitly rejected by the glossary. If a required concept is absent, reconsider the terminology or record the gap for `domain-modeling`.

## ADR conflicts

Surface conflicts with existing ADRs explicitly instead of silently overriding them.
