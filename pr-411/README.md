# PR for #411 — Classic EditText doubled border

Captured on the Classic rail (MAME `maciix`, RAM 8M) from
`tests/toolbox/run-scenario.sh helloworld bmi-roundtrip`.

`bmi-roundtrip` is a tracked scenario whose EditText controls sit below the
420x300 window fold (#404), which is why no golden shows this defect. To make
them visible the window was temporarily enlarged to 420x440 **in the working
tree only** — that change is not part of the PR and no golden was re-recorded.

Both runs are identical except for the fix:

- the durable audits are byte-identical, so nothing audit-visible changed
- both differ from the tracked expected audit at the same byte (837, line 10),
  which is the temporary window size, not the fix
- 544 of 307,200 pixels differ

`*-3x.png` are the EditText region at 3x, cropped from the full captures.

## Correction (2026-08-21)

The paragraph above says the EditText controls "sit below the 420x300 window
fold (#404)" on this rail. **That is wrong for Classic.** Measured against the
archived Classic golden set (`golden/classic-maciix-2026-08-17/helloworld/`):

The window's content box is x50-469, y71-370. Every element of the BMI section
is inside it:

| band | element |
| --- | --- |
| y206-214 | `BMI Calculator` |
| y225-237 | `Height (cm)` |
| y243-262 | EditText (height) |
| y265-277 | `Weight (kg)` |
| y283-302 | EditText (weight) |
| y306-314 | BMI result |

56 px of the content box are left over below the last element. And
`startup.png` vs `bmi-roundtrip.png` in that archived set differ by 746 px at
y86-94, y245-261, y285-301 and **y306-314 x100-124** — the last of which is the
result text changing. On Classic the BMI section is drawn, captured, and
compared.

Two consequences for the captures in this directory:

- The temporary enlargement to 420x440 was not needed to make the controls
  visible. Its band layout is identical to 420x300; the extra 140 px is empty.
- 420x440 does not fit the 640x480 screen. In both `bmi-roundtrip-before.png`
  and `bmi-roundtrip-after.png` the window's white content runs to y=479, so
  the window's bottom border is off-screen.

**The fold problem #404 describes is real, but on the Win32 rail, not Classic.**
Measured 2026-08-21 on Omen at `main = 22181b0b`: the Win32 window at 420x300
cuts off after the `Height (cm)` label, so the height EditText, `Weight (kg)`,
the weight EditText and the result are all below the fold, and the settled
capture for `helloworld bmi-roundtrip` is byte-identical to `helloworld
startup`. The #450 guard refuses to record it.

**None of this affects the #411/#434 evidence.** The 544-pixel difference these
captures were taken to show is in the EditText frames themselves, which are
visible on Classic at either window size.
