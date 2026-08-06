---
name: codex-delegate
description: Delegate routine implementation or GitHub work to the Codex CLI without the known traps — stdin, orphan processes, connector write limits. Use whenever launching codex exec from a session.
---

# Delegating to Codex CLI

Fleet roles: Fable = design/review/commit, Codex = routine implementation and
GitHub operations, Sonnet = domain research, Haiku = cross-tree sweeps. Codex
works in the fleet clone `~/loka` (WSL ext4), never in the user's `/mnt/c`
checkout; sync goes through origin only.

## Launch rules (each one paid for in lost time)

- **Background `codex exec` MUST redirect stdin**: append `< /dev/null`.
  With stdin open it prints "Reading additional input from stdin..." and
  waits forever (32 minutes lost once).
- **Full output to a file, never `| tail`** — piping hides progress and
  truncates the execution evidence.
- **Before re-issuing a request, check for orphans**: `pgrep -af codex`.
  A previous run may still be alive; non-idempotent operations (pushes, issue
  comments, file moves) risk double execution.
- **Outside a git repository, pass `--skip-git-repo-check`.** Running from
  `/tmp` otherwise dies with "Not inside a trusted directory". This comes up
  whenever the point of the run is to keep Codex *away* from the repository —
  e.g. re-implementing something from its spec alone as an independence check.
- `--full-auto` is deprecated; use `--sandbox workspace-write`.

## Connector limits

- Codex **reads** Notion fine, but **cannot write**: the app-connector write
  approval is auto-cancelled in non-interactive runs, and `--full-auto` does
  not override it. For a Notion write, an interactive codex session with one
  manual approval is the shortest path.

## Always ask for the smell list (required deliverable)

Every implementation brief must require, as a delivery item alongside the code
and the PR body, a `.<slice>-smells.md` naming the places the delivered shape
reads as bolted-on. Word it so the list cannot come back empty by default:

> List at least three places where this change reads as bolted-on, ranked, with
> the reasoning for each: a door or field that duplicates the shape of an
> existing one; a check whose absence in some build configuration is worse than
> "undetected"; a step added to one reclamation or teardown path that other
> paths reaching the same point still lack; a field a type never reads itself;
> an API shape chosen for test observability. If you believe the shape is
> clean, say so per item and explain what you considered — "nothing found" with
> nothing considered is not an accepted answer.

Reason for the forced output: a passive escape hatch does not fire. The S3
brief said "stop and write a questions file if the rulings underdetermine
something"; the file came back absent while the review found three real holes,
one of them exactly an underdetermined ruling. This mirrors AGENTS.md
"Shape Review Gates" gate 2, which the delegator runs on the same diff.

## Reviewing what comes back

- Treat Codex reports as claims: independently re-verify key results
  (test counts, exit codes) rather than quoting them.
- Read the smell list before reading the diff, then review the diff without it
  — the list is a hypothesis set, not a substitute for looking.
- The Codex PR-review bot's comments on GitHub PRs must always be read and
  answered before merge.
