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
- **Do not wait on `pgrep -f "codex exec"`** — the pattern matches the waiting
  shell's own command line, so the loop reports "still running" forever (two
  finished runs were watched for over an hour, 2026-08-30). Wait on the log
  instead: the brief's `===== REPORT =====` marker, or an `echo "codex
  exit=$?"` appended after the exec. `pgrep -af codex` is still right for the
  orphan check *before* a launch, where a false positive only costs a look.
- **`--sandbox workspace-write` blocks the network, so `git push` and `gh pr
  create` always fail** — the sandbox only allows `[workdir, /tmp, $TMPDIR]`
  and no outbound traffic. Left to its own devices Codex clones into `/tmp`,
  re-points `origin` at the GitHub URL, runs `gh auth status`, and reports a
  `gh could not authenticate` / "run `gh auth refresh`" error. **That error is
  the sandbox, not a real auth problem.** Do not chase it. **Every
  implementation brief must say: commit and STOP; do not push, do not `gh pr
  create` — the delegator pushes and opens the PR.** Then recover the commit
  (it is on the branch Codex made, and/or as uncommitted changes on the
  working clone's HEAD — check both) and push it yourself from a worktree.
  Seen on #488, #491.
- **In a linked worktree Codex usually cannot commit at all.** A worktree's
  `.git` is a file pointing at the parent clone's `.git/worktrees/<name>/`,
  which lies outside the sandbox's write root, so `git commit` dies with
  `index.lock: Read-only file system`. An individual run occasionally succeeds
  (environment variation), so brief for both outcomes: **"commit if you can; if
  the commit fails, stop and leave the work dirty, with the results in
  `FINDINGS.md`"** — the delegator commits at harvest either way. The split is
  not only a workaround: nothing lands without having been read. Measured
  2026-08-28.

## Resuming instead of re-briefing

A run that came back incomplete — a leg skipped, the smell list missing, a
question it should have asked — is cheaper to continue than to re-issue.
`codex exec resume <SESSION_ID> "<follow-up>"` replays that session's context,
so the follow-up does not re-pay for the repository walk the first run already
did; `--last` picks the most recent session for this cwd (`--all` disables the
cwd filter). `codex exec fork <SESSION_ID>` branches a session instead, for
trying a second approach without spending the first. Both subcommands are
verified against `codex-cli 0.149.1`; `fork` did not exist under `exec` in
older 0.144 alphas, so check `codex exec --help` before writing a brief around
it on an older CLI.

Every launch rule above still applies to a resume — `< /dev/null`, full output
to a file, and orphan check first. The orphan check matters more here, not
less: a resume that races the original run's leftovers double-executes whatever
the follow-up asks for. Session ids are in the transcripts under
`~/.codex/sessions/<JST date>/rollout-*.jsonl`.

## Connector limits

- Codex **reads** Notion fine, but **cannot write**: the app-connector write
  approval is auto-cancelled in non-interactive runs, and `--full-auto` does
  not override it. For a Notion write, an interactive codex session with one
  manual approval is the shortest path.

## Briefing a change that adds tests

- **A new test header must be included in both test mains**:
  `tests/ContractTestMain.cpp` *and* `tests/FlowDslTestMain.cpp`. The
  macOS and Win32 test targets compile only the second one, so a header
  included only in the first passes Linux CI and fails macOS/Win32 CI (PR
  #532, one extra round trip). Put the sentence in the brief; Codex does not
  find the second main on its own.

## Always ask for the smell list (required deliverable)

Every implementation brief must require, **as a section of the PR body**, a
ranked list naming the places the delivered shape reads as bolted-on. AGENTS.md
"Shape Review Gates" gate 2 item 5 puts the findings in the PR body; a scratch
file while working is fine but **must not be committed** — say so in the brief.
Asking for a `.<slice>-smells.md` deliverable without that caveat put four
stray markdown files on the ScrapbookUI stack and cost a rebuild to remove
them. Word the requirement so the list cannot come back empty by default:

> List at least three places where this change reads as bolted-on, ranked, with
> the reasoning for each: a door or field that duplicates the shape of an
> existing one; a check whose absence in some build configuration is worse than
> "undetected"; a step added to one reclamation or teardown path that other
> paths reaching the same point still lack; a field a type never reads itself;
> an API shape chosen for test observability. Separately, enumerate every
> primitive member (`bool`, counter, index) the change adds to an existing
> type, each with its single writer, every reader outside the owning type,
> and the existing phase, type, or return value it could not be derived from
> — an added flag or counter is returned for reshaping unless that line
> exists, and a foreign reader fails it even when the writer is unique. If you believe the shape is
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

- **`--sandbox read-only` also blocks the deliverable file.** A REFUTE/audit
  run briefed to write `FINDINGS.md` under `~/codex-briefs` finished its whole
  audit and then failed the single write (2026-08-30, 277k tokens). For
  read-only audits either use `--sandbox workspace-write -C ~/codex-briefs`
  with an explicit "do not modify the repository" instruction, or brief the
  run to print the document to stdout (the log captures it). Recovery is
  `codex exec resume <SESSION_ID> "print the complete findings to stdout"` —
  the session id is the first line of the exec log (`session id: ...`).
