## Testing

At the end of a change, run `scripts/build-windows.ps1`, then `scripts/test-all.ps1`. `test-all.ps1` runs every `scripts/test-*.ps1` and exits non-zero if any fail.

## Agent skills

### Issue tracker

Issues and specs are tracked in this repository's GitHub Issues. See `docs/agents/issue-tracker.md`.

### Triage labels

Use the five default triage labels: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, and `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

This is a single-context repository. See `docs/agents/domain.md`.
