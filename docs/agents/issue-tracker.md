# Issue tracker: GitHub

Issues and PRDs for this repo live as GitHub issues. Use the `gh` CLI for all operations.

## Conventions

- Create: `gh issue create --title "..." --body "..."`
- Read: `gh issue view <number> --comments`
- List: `gh issue list --state open --json number,title,body,labels,comments`
- Comment: `gh issue comment <number> --body "..."`
- Label: `gh issue edit <number> --add-label "..."` or `--remove-label "..."`
- Close: `gh issue close <number> --comment "..."`
- Infer repository from the current clone and `git remote -v`.

## Pull requests as a triage surface

**PRs as a request surface: no.**

## Skill operations

- “Publish to the issue tracker” means create a GitHub issue.
- “Fetch the relevant ticket” means run `gh issue view <number> --comments`.
- Bare references such as `#42` may identify an issue or PR; resolve using `gh pr view 42`, then fall back to `gh issue view 42`.
- Wayfinder maps use label `wayfinder:map`; child tickets use `wayfinder:<type>`.
- Prefer GitHub sub-issues and native issue dependencies. If unavailable, use task-list links and `Blocked by: #<number>` metadata.
- Claim work using `gh issue edit <number> --add-assignee @me`.
