# MineSweeper retained Cell rebind (#615)

Status: Build-verified; native corrected/mutation runs and measured audits pending.
Owns: The Toolbox-only retained Cell probe in the existing MineSweeper driver.
Code truth: `src/MineSweeperScenarioDriver.cpp`, `src/RetainedRebindScenario.hpp`.

`retained-cell-rebind` mounts the deterministic production scene in a 220x240
window at (20,45), keeping its structure below the menu bar and within the
640x480 rig. Only this new cell overrides the driver presentation frame.

At the second settled idle turn it resolves `MineSweeper.Cell.0`, then applies
text/click sources A and B through CellDefinition::applyPropsToNode on the same
node and context. The driver AppConfig owns both text States and emitters until
after App/Scene destruction. A TEST_BUILD read-only query copies the existing
CellHit; both cached sources must be B before render. Dirty replay must match
every pixel of an erased direct context draw without increasing full renders.
The probe routes a window-local click through the controller to the Cell; a
stack-scoped observer pair requires exactly one B event and zero A events.
The controller's mouse return value denotes edit focus, not Cell event success.
The observer pair unbinds synchronously before leaving the test operation.

Reapplying B must add zero refresh calls and rows. Checkpoints are `apply-A`,
`B-hit-and-replay-before-render`, `click-reaches-B-only`, `unchanged-B`; each
records cumulative `refresh.calls` and `refresh.rows` through ScenarioAudit.

Cell paint and click currently read context/node props directly, so pixels and
click alone cannot detect a stale CellHit. The row query independently checks
its cached sources; this is why it is necessary in addition to exercising the
production replay and click doors. No example code changes or production
lifetime fields are added. The probe is Toolbox-only and excluded from the
portable Flow reel. The delegator measures expected audits and the mutation
matrix; the runner's strict missing-audit refusal remains unchanged.

The driver publishes the final verdict through its existing scenario terminal
owner. For these native-only cells it skips portable scenario.stop() during
config destruction: that unused Flow must not run cancellation steps after the
native audit terminal. Its existing FlowSlot destructor releases the unstarted
chain. This test-driver teardown selection is provisional until the rig verifies
the final audit has one terminal and no records after it.
