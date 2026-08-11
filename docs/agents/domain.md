# Domain Docs

This repository uses the single-context layout.

## Before exploring

Read these when they exist:

- `CONTEXT.md`
- Relevant ADRs under `docs/adr/`

If they do not exist, proceed silently. Domain-modeling workflows create them lazily when terminology or decisions are resolved.

## Layout

/
├── CONTEXT.md
├── docs/
│   └── adr/
└── src/

## Vocabulary

Use domain terms exactly as defined in `CONTEXT.md`. If a required concept is missing, reconsider whether the term belongs or record the gap for domain modeling.

## ADR conflicts

Surface conflicts with existing ADRs explicitly instead of silently overriding them.
