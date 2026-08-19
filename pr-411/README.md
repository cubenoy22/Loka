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
