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

Two sibling .NET repositories consume this functionality over TCP (default `127.0.0.1:5150`):

- `../amnesia-multiplayer` is the primary consumer. Its game-facing client lives under `src/Multimnesia.Client`.
- `../streamnesia` (`Streamnesia.Execution.AmnesiaClient`) uses the legacy protocol and pins an older build of this repository. The legacy protocol is intended to be retired (ADR 0004); if Streamnesia upgrades, it moves to the negotiated protocol.

When changing the TCP protocol, connection lifecycle, command handling, responses, events, encoding, framing, or default endpoint:

1. Inspect the corresponding client behavior in `../amnesia-multiplayer`, and in `../streamnesia` when touching the legacy protocol.
2. Keep Sessions that do not negotiate a Protocol Version on the legacy protocol byte-for-byte until it is deliberately retired.
3. Account for required coordinated changes or migration work in the consuming repositories.
4. Keep each repository's issue tracking and changes scoped to its own repository.

## Use the glossary's vocabulary

When output names a domain concept—in an issue title, refactor proposal, hypothesis, or test name—use the term defined in `CONTEXT.md`. Avoid synonyms the glossary explicitly rejects.

If a required concept is absent, reconsider whether the project already uses another term or note the genuine gap for `/domain-modeling`.

## Flag ADR conflicts

If output contradicts an existing ADR, surface the conflict explicitly rather than silently overriding it.
