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
  finished runs were watched for over an hour, 2026-08-30). Wait on a
  process-completion sentinel in the log instead. The `===== REPORT =====`
  marker alone is not one — a run that dies before printing it leaves the
  loop waiting forever — so write the exit status from the same subshell
  that ran the exec:

  ```sh
  ( codex exec --sandbox workspace-write -C "$WT" "$(cat brief.md)" \
      < /dev/null > run.log 2>&1; echo "codex exit=$?" >> run.log ) &
  ```

  `codex exec ... & echo "codex exit=$?"` is wrong: it records the launch
  status immediately, not Codex's. Then wait for `grep -q '^codex exit='
  run.log` — it fires on success and on abnormal exit alike. `pgrep -af
  codex` is still right for the orphan check *before* a launch, where a false
  positive only costs a look.
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
to a file, and orphan check first. **Global exec flags go BEFORE the
subcommand**: `codex exec --sandbox workspace-write -C <dir> resume
<SESSION_ID> "<follow-up>"`. Putting `--sandbox`/`-C` after `resume` dies
instantly with `error: unexpected argument` (one launch lost, 2026-08-31);
the resume parser accepts many exec options of its own (`-m/--model`,
`-i/--image`, `--enable/--disable`, `--skip-git-repo-check`, …) but NOT
`--sandbox` or `-C` — check `codex exec resume --help` for the exact list on
your CLI version rather than assuming, and a cheap syntax probe before the
real launch is
`codex exec <flags> resume --help > /dev/null; echo $?`. The orphan check matters more here, not
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

## Pre-PR completion gate (run before the PR exists, not after the bot)

PR #589 (2026-09-03) took ten PR-review-bot rounds, one macOS compile
failure and one Win32 CI hang to converge on a change whose host suite was
green from round one. Every one of those rounds was reachable locally. The
gate below runs on the final diff — Codex's work plus every edit the
delegator added afterwards — before the branch is pushed.

1. **State the invariant of anything deferred, queued, cached, or
   ledgered in one sentence, then enumerate its negations and pin each.**
   For #589 the sentence was "a pending entry is valid only for a surface
   this pass actually placed and that is still placed when delivered; a
   newer accepted pass supersedes it; a row leaves the ledger before app
   code runs". Its negations were the bot's rounds 2–8: leaf refusal or
   null native handle, ancestor refusal after child success, a nested
   pass superseding older rows, `DETACHED_RETAINED` or `RETIRED` before
   delivery, node absence without a replacement row, cancellation during
   the delivery callback. Put the sentence and the negation list in the
   implementation brief; a brief without it is not ready.
2. **Adversarial pass on the final diff, in a fresh session.** Either
   `codex review --base <base>` from the worktree (the same reviewer the
   PR bot runs, non-interactive, log to a file with the `codex exit=`
   sentinel) or a read-only `codex exec` REFUTE brief with the invariant
   and its negations as numbered claims. A same-session self-review is not
   accepted: the #589 smell report noticed the raw node pointer and then
   argued it safe. Every REFUTED item is fixed and the pass rerun clean
   before the push.
3. **Native compile leg for every rail the host cannot build.** Pushing
   the branch as transport is fine and often required — the rigs fetch
   through origin (fleet rule) and tahoe can also take an scp'd diff; the
   gate is drawn at opening the PR and asking the bot, not at `git push`. macOS:
   tahoe `cmake --preset macos-debug && cmake --build --preset macos-tests`
   (~3 min). Win32: the rig's `win32-tests` build plus `LokaTestsWin32` in
   an interactive scheduled task (~5 min; an MSVC Debug assert or a
   use-after-destruction hangs on the abort dialog, so run under a timeout
   and read the log). 68K/PPC: the local Retro68 presets. CI is not the
   first compiler.
4. **Null-rail test shape.** The `NullScenePlatformController` is the first
   declaration in a test that projects nodes (it must outlive the nodes:
   their terminal fact delivery reaches it during teardown; declared after
   the nodes it is a dead stack object that Linux and ASan still tolerate
   and MSVC Debug hangs on). Mind the assert-audit
   coupling: `LOKA_VERIFY(x.call())` registers that call name as
   load-bearing in `tools/ci/check_test_asserts.py`, which then reds every
   plain `assert(y.call())` of the same name in other files. Either bind
   the result to a local and verify the local, or convert those plain
   asserts to `LOKA_VERIFY` in the same change — never leave the audit
   half-converted, and never weaken a `LOKA_VERIFY` back to `assert` to
   silence it (`assert` vanishes under `NDEBUG`; `LOKA_VERIFY` is the one
   that keeps evaluating). Both test mains
   include the new header.
5. **Then open the PR and ask the bot once.** A finding after this gate is
   classified before it is fixed: brief gap, self-findable miss,
   delegator's later edit, or pre-existing shape routed to its issue.

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
