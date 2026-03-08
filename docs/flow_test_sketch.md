# Flow-Based UI Test Sketch

Exploration of reusing the Loka Flow DSL for automated UI testing.

## Concept

Flow's `Step` / `goto` / `loop` / error handling maps directly to test scenario description. On failure, `Dump()` serializes the entire flow state for debugging.

## API Sketch

```cpp
TestFlow(testState)
  | Step(WAIT_WINDOW, WaitFor(WindowVisible("Main")))
  | Step(CLICK_BTN,   MoveTo(Button("OK")).then(Click()))
  | Step(VERIFY,      Capture().expect(TextEquals("Done")))
  | onFailure(Dump(testState), ABORT);
```

## Alignment with Existing Design

### Flow Primitives Reused Directly

- `Step` for sequential actions
- `goto` / `loop` for retries and iterative scenarios
- Timeout and error handling built into the flow model
- No new control-flow concepts needed

### Dump() for Failure Diagnostics

- Serializes which Step was reached and the value of each State at the point of failure
- Flow state is already structured data — no extra instrumentation required
- Reports can be assembled from a sequence of `Capture()` + `Dump()` results

### 68k Self-Testing

- Typical GUI test automation frameworks assume modern environments
- Flow-based testing runs on the same runtime as the app itself, enabling self-tests on 68k/Classic

## Status

Conceptual. Low implementation priority.
