# Issue tracker: GitHub

Issues and PRDs for this repository live in GitHub Issues. Use the `gh` CLI for all operations.

## Conventions

- Create: `gh issue create --title "..." --body "..."`
- Read: `gh issue view <number> --comments`
- List: `gh issue list --state open --json number,title,body,labels,comments`
- Comment: `gh issue comment <number> --body "..."`
- Label: `gh issue edit <number> --add-label "..."` or `--remove-label "..."`
- Close: `gh issue close <number> --comment "..."`

Infer the repository from `git remote -v`.

## Pull requests as a triage surface

**PRs as a request surface: no.**

## Publishing

When a skill says “publish to the issue tracker”, create a GitHub issue.

When a skill says “fetch the relevant ticket”, run:

`gh issue view <number> --comments`

## Wayfinding operations

- The map is one issue labelled `wayfinder:map`.
- Child issues use `wayfinder:research`, `wayfinder:prototype`, `wayfinder:grilling`, or `wayfinder:task`.
- Prefer native GitHub sub-issues and issue dependencies.
- If those APIs are unavailable, use task lists and `Blocked by: #<number>` lines.
- Claim work with `gh issue edit <number> --add-assignee @me`.
- Resolve a decision by commenting the evidence, closing the issue, and updating the map’s Decisions-so-far section.
