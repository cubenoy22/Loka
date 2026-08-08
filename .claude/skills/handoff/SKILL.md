---
name: handoff
description: Write the end-of-session handoff document (plans/HANDOFF-YYYY-MM-DD.md) so the next session can resume without re-derivation. Use when the user asks for a handoff or the session is wrapping up.
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
