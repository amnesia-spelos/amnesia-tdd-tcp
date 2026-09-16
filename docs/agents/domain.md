# Domain Docs

How the engineering skills should consume this repo's domain documentation when exploring the codebase.

## Before exploring, read these

- **`CONTEXT.md`** at the repository root.
- **`docs/adr/`**: read ADRs that touch the area you're about to work in.

If these files do not exist, proceed silently. The `/domain-modeling` skill creates them lazily when terms or decisions are resolved.

## File structure

This is a single-context repository:

```text
/
├── CONTEXT.md
├── docs/adr/
└── src/
```

## Cross-repository integration

This repository is the Amnesia: The Dark Descent game fork that exposes external game interaction and control over TCP.

The sibling `../streamnesia` repository is a .NET consumer of this functionality. Its `Streamnesia.Execution.AmnesiaClient` connects to the game through TCP; the checked-out configuration defaults to `127.0.0.1:5150`.

When changing the TCP protocol, connection lifecycle, command handling, responses, events, encoding, framing, or default endpoint:

1. Inspect the corresponding client behavior in `../streamnesia`.
2. Treat compatibility with that client as part of this repository's contract.
3. Account for required coordinated changes or migration work in both repositories.
4. Keep each repository's issue tracking and changes scoped to its own repository.

## Use the glossary's vocabulary

When output names a domain concept—in an issue title, refactor proposal, hypothesis, or test name—use the term defined in `CONTEXT.md`. Avoid synonyms the glossary explicitly rejects.

If a required concept is absent, reconsider whether the project already uses another term or note the genuine gap for `/domain-modeling`.

## Flag ADR conflicts

If output contradicts an existing ADR, surface the conflict explicitly rather than silently overriding it.
