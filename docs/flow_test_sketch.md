# Flow-Based UI Test Sketch

Exploration of reusing the Loka Flow DSL for automated UI testing.

## Scope and Security Model

- Default scope is **Scene inside the app window only**.
- Global UI (menu bar, app-external UI) is not part of default capture.
- Privileged/global inspection is optional and runs only when platform capability is available.
- If global inspection is unavailable, tests must report `SKIP` (not `FAIL`).

## Concept

Flow's `Step` / `goto` / `loop` / error handling maps directly to test scenario description. On failure, `Dump()` serializes the entire flow state for debugging.

`NextTickTracker` is used as the deterministic observation boundary:
- `update request -> next tick flush -> capture/assert`
- Multiple updates in the same tick are treated as one flush unit.

## API Sketch

```cpp
TestFlow(testState)
  | Step(WAIT_WINDOW, WaitFor(WindowVisible("Main")))
  | Step(UPDATE_TEXT, SetState(textState, "long long long..."))
  | Step(WAIT_TICK,   WaitNextTick())
  | Step(CAPTURE,     CaptureScene().expect(TextHeightIncreased("MainText")))
  | onFailure(Dump(testState), ABORT);
```

Privileged probe sketch:

```cpp
if (probe.canInspectGlobalUi()) {
  CaptureGlobal().expect(MenuItemExists("File/Quit"));
} else {
  Skip("global ui probe unavailable");
}
```

## Capture Output Policy

- Default output root: `./captures/` (same directory level as executable).
- Per-run directory: `LokaTest-<locale>-<timestamp>/`.
- File-safe timestamp format only (no `:` or `/`).
- Toolbox/retro targets should enforce limits:
  - max files
  - max total bytes
  - delete oldest first when limit is exceeded

## Config Hook (`LokaTest.cfg`)

When present in the executable directory, load optional test settings:

- `capture_dir`
- `max_files`
- `max_total_bytes`

If missing, run with defaults.

## CI Considerations

- CI is the primary execution environment.
- CI jobs should set `capture_dir` to a workspace path collected as artifact.
- Test results should separate `PASS/FAIL/SKIP` explicitly.
- Runtime verification for text relayout should include:
  - short -> long text
  - long -> short text
  - resize + text update

## Alignment with Existing Design

### Flow Primitives Reused Directly

- `Step` for sequential actions
- `goto` / `loop` for retries and iterative scenarios
- Timeout and error handling built into the flow model
- No new control-flow concepts needed

### Dump() for Failure Diagnostics

- Serializes which Step was reached and the value of each State at the point of failure
- Flow state is already structured data, so extra instrumentation is minimal
- Reports can be assembled from a sequence of `Capture()` + `Dump()` results

### 68k Self-Testing

- Typical GUI test automation frameworks assume modern environments
- Flow-based testing runs on the same runtime as the app itself, enabling self-tests on 68k/Classic

## Status

Conceptual. Prepare now, implement in phases after core behavior stabilizes.
