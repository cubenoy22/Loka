# HelloWorld retained rebind scenarios (#615)

Status: Build-verified; native corrected/mutation runs and measured audits pending.
Owns: The two Toolbox-only retained-apply probes in the existing HelloWorld driver.
Code truth: `src/HelloWorldScenarioDriver.cpp`, `src/RetainedRebindScenario.hpp`.

Both cells use the production scene in the existing 420x330 window at (50,50),
which fits the 640x480 rig. They run at the second settled idle turn and complete
their checks synchronously before another render can repair a stale projection.
The driver AppConfig owns the fixture States and item vectors until after App
and Scene destruction; neither fixture changes the example source.

`retained-edittext-rebind` resolves the first EditText under `HelloWorld.Bmi`,
the existing BMI height selector. It applies text A (171), then B (182) through
EditTextDefinition::applyPropsToNode and requires the existing TE record to
contain 182 immediately. It uses the existing read-only TE query, which does
not synchronize before reading. Changing A to 199 must produce no native edit
notification and leave TE at 182; changing B to 183 must produce exactly one
notification and update TE. Reapplying B must add zero refresh calls and rows.
Checkpoints: `apply-A`, `B-in-TE-before-render`, `A-does-not-notify-TE`,
`B-notifies-TE`, `unchanged-B`.

`retained-popup-rebind` resolves `HelloWorld.RightPanel.FruitPopup`. It applies
borrowed item vector/selectedIndex/enabled sources A, then B, through
PopupMenuDefinition::applyPropsToNode. A read-only TEST_BUILD query copies the
existing hit row; all three source identities must be B. Dirty replay of that
row must match every pixel of a freshly erased, direct context draw, and the
full-render counter must not advance. B selects Pear B from its new vector.
Reapplying B adds zero refresh calls and rows. Checkpoints: `apply-A`,
`B-hit-and-replay-before-render`, `unchanged-B`.

Every checkpoint records cumulative `refresh.calls` and `refresh.rows` in the
normal ScenarioAudit record. Every apply also requires retained context identity.
No expected audit is fabricated: the registry and runner continue to require
tracked, measured audits. These native probes do not join the portable Flow reel.
The delegator must measure corrected runs and omission, wrong-ledger,
unconditional-capture, and omitted-controller-call mutations before claiming
runtime discrimination. The final pixel golden alone cannot detect wasted
refresh work; the unchanged apply assertions provide that side of the gate.

The driver publishes the final verdict through its existing scenario terminal
owner. For these native-only cells it skips portable scenario.stop() during
config destruction: that unused Flow must not run cancellation steps after the
native audit terminal. Its existing FlowSlot destructor releases the unstarted
chain. This test-driver teardown selection is provisional until the rig verifies
the final audit has one terminal and no records after it.
