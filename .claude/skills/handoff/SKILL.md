---
name: handoff
description: Write the end-of-session handoff document (plans/HANDOFF-YYYY-MM-DD.md) so the next session can resume without re-derivation, then mirror it to the Loka Handoff Archive in Notion when available. Use when the user asks for a handoff or the session is wrapping up.
---

# Session handoff document

Write `plans/HANDOFF-<today>.md` in the user's checkout
(`/mnt/c/Users/cuben/source/repos/Loka`). `plans/` is gitignored — this is a
local document, written in Japanese (the user reads it directly).

## Required structure (follow the existing files as templates)

1. **Title line**: one parenthesized phrase naming the session's outcomes.
2. **前ハンドオフ**: link the previous HANDOFF file and one line on how this
   session continues it. Record the resulting `main` SHA.
3. **作業場所**: which clone/worktrees were used (fleet `~/loka` vs `/mnt/c`),
   including branches — the next session must not guess.
4. **状態サマリ**: a table of item → state (merged SHAs, open PRs, blocked
   items).
5. **中身とエビデンス**: per deliverable, the verification actually run
   (test counts per host, red→green mutation evidence, real-machine runs)
   with file/log locations.
6. **次にやること**: numbered, most-actionable first, with the concrete first
   step of each.
7. **判断待ち**: decisions only the user can make, carried forward explicitly
   so they are not silently dropped.
8. **事故と教訓**: anything that cost time this session and the rule that
   avoids it next time. Promote recurring ones to memory or a skill.

## Rules

- Convert every relative date ("yesterday", "next time") to absolute dates.
- State what was NOT done or NOT verified as plainly as what was.
- If cleanup was performed (worktrees, temp scripts, branches), list it so
  the next session doesn't hunt for ghosts; if cleanup remains, say so.

## Notion mirror

Run this phase only after the local file is complete. The local file is the
completion boundary — a Notion outage or missing connector must not
invalidate the handoff. On any failure, report `Notion mirror: pending` with
the exact reason and stop; a later session can retry by Handoff Key.

1. Fetch the [Loka handoff hub](https://app.notion.com/p/3b9d0e70af0180cc837ccb088ee65295)
   and its `Loka Handoff Archive` database on every run instead of assuming
   the schema (expected data source:
   `collection://3b9d0e70-af01-8007-b67a-000b4f01badf`; prefer the fetched
   value if it changed).
2. Query the data source for an exact `Handoff Key` match — the key is the
   local filename including `.md` (e.g. `HANDOFF-2026-08-12.md`). No match:
   create one record. Exactly one: update its properties and replace its
   body. Multiple: change nothing and report the duplicate-key problem.
3. Properties: `Title` = `YYYY-MM-DD — <concise topic>`; `Session Date`;
   `Agent` = `Claude`; `Handoff Key` as above; `Topic` = concise search
   terms; `Summary` = one sentence (done-state + continuation point);
   `Branch`/`HEAD`/`Base Commit` = verified Git facts (recorded main SHA for
   Base Commit); `Working Tree` = clean/dirty/unknown; `Evidence` = only
   applicable values of runtime-verified / build-verified / red-green /
   not-verified; `Status` = Ready/Partial/Blocked; `Local File` =
   repo-relative `plans/...` path.
4. Page body = the local handoff minus its title line (the `Title` property
   supplies it). Do not upload secrets, tokens, PII, or LAN addresses —
   refer to rigs by their logical names (e.g. `tahoe`, `mavericks-legacy`).
5. Verify: fetch the resulting record back, and report its URL.
