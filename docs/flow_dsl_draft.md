# Flow DSL Draft (Temporary)

## Goal

Define a Slot-DSL-aligned, type-safe pipeline DSL for async/resource flows (file -> blob -> image) with:

- C++98 compatibility
- no exceptions
- explicit error handling
- step-based state machine transitions
- optional loading/error state outputs
- a unified `Step(...)` model for bounded-lifetime transform pipelines (file/network/decode/encode)

This is a draft spec for incremental implementation.

## Non-goals (for first iteration)

- full Promise API parity
- background thread scheduler design
- generic cancellation propagation across all subsystems

## Core concepts

### 0) Evaluation model

Flow DSL should follow existing Loka DSL execution style.

- Expression nodes/functors are composed via templates at compile time.
- Runtime evaluation uses a Flow-specific lightweight slot-dispatch context (EvalContext-like pattern).
- Definitions are owned by arena/list structures (no heavy runtime VM design).

Practical implication:

- Type-chain checks are compile-time.
- Runtime path should be flattened into simple step dispatch and pointer-based evaluation.
- Split large flows to keep per-flow template depth small.
- Do not couple Flow runtime to existing Expr DSL `EvalContext` directly; keep Flow context independent.

### 1) Step typing

Each step is modeled as:

- `Step<In, Out>`

Composition requires exact type match:

- `Step<A, B> | Step<B, C> -> Step<A, C>`

Type mismatch must fail at compile time.

Type resolution rules (v1):

1. `Step(id, adapter)` fixes `Out` to `adapter::Result`.
2. `Step(id, adapter).input(&state)` fixes `In` to the bound state value type.
3. If `.input(...)` is omitted on a non-first step, `In` is inherited from previous step `Out`.
4. If `.input(...)` is omitted on the first step, compilation must fail.
5. v1 uses one adapter per step (no multi-converter step chaining inside one step).

Step-as-node rule:

- `Step(id, adapter)` is the core node unit in v1.
- Adapters may represent:
  - pure conversion (`FileToBlobAdapter`)
  - async boundary (`HttpGetAdapter`)
  - bounded processing stage (`DecodeFrameAdapter`, `VideoEncoderAdapter`)
- In other words, v1 keeps one step chain model for short/finite transform flows.
- Long-lived runtime loops (for example always-on players) are out of v1 scope.
- Keep v1 focused on pipelines that naturally terminate, so arena bulk-free remains valid.

Step boundary rule:

- Steps are delimited by `|` in the flow chain.
- Do not use `endStep()` style terminators.
- Configure a step with chained methods/operators inside the step segment.

### 1.1) Step run status (v1)

Adapter execution is tri-state:

```cpp
enum StepRunStatus
{
  FLOW_STEP_PENDING = 0,
  FLOW_STEP_SUCCEEDED = 1,
  FLOW_STEP_FAILED = 2
};
```

- `SUCCEEDED`: continue to next step.
- `FAILED`: enter local `onFailure` chain.
- `PENDING`: pause flow without terminal cleanup and wait for explicit `resume(StepId)`.

### 2) Step-first composition (v1)

v1 prefers step-first composition without placeholder syntax.

- Avoid `_0`/slot placeholder usage in v1 surface syntax.
- Encode transitions as explicit steps and handlers.
- Use handler-attached step destinations and `resume(...)` for routing.

### 3) Resumable labels

Prefer step IDs over string labels for resume/state-machine integration.

Any step may expose a typed step ID:

- `StepId`

Runtime API:

- `resume(StepId id)`
- `runResult()` / `resumeResult(StepId id)`

Flow run result is also tri-state:

```cpp
enum FlowRunResult
{
  FLOW_RUN_PENDING = 0,
  FLOW_RUN_SUCCEEDED = 1,
  FLOW_RUN_FAILED = 2
};
```

If ID is not reachable/valid, return failure result (no throw).

String labels are optional debug metadata only:

- `Resumable("decode")`

## Flow ownership boundary

`Flow` itself should only define chained state transitions.

- no mandatory external side effects
- no mandatory final result write
- no implicit global state mutation

External coordination must be explicit via optional bindings:

- `onSuccess(MutableState<T>*)`
- `onError(MutableState<E>*)`
- `trackLoading(MutableState<bool>*)`

All binding targets are nullable and optional.
If all are null, flow still executes as an internal state machine.

Ownership/lifetime rule (v1):

- Every flow runtime must have an explicit owner.
- Preferred owner models are:
  1. `Window/Scene/Boundary`-scoped owner (UI-local flow)
  2. repository/global cache owner (app-lifetime flow)
- DI container integration is not required in v1.

v1 safety rule (simple):

- Owner destruction => runtime `detach/cancel` => internal cleanup.
- Runtime destructor is final fallback cleanup.
- After detach, all bound-state writes are no-op.
- Do not place flow ownership on short-lived inner nodes unless immediate teardown is desired.
- Steps requiring timers (for example `timeoutMs`) must receive timer access through owner/platform context.

Dynamic view note:

- For frequently recreated inner views, prefer owning flow at a higher boundary level.
- Avoid tying flow ownership to short-lived inner nodes unless aggressive memory release is explicitly required.

## Memory model

- Flow expression/runtime nodes are cloned into a Flow-owned arena.
- Flow teardown implies arena bulk free (no individual delete in normal path).
- This reduces fragmentation risk on constrained heaps.
- Arena lifetime should match Flow runtime instance lifetime.
- Structural cleanup hooks (including loading cleanup) should attach to this lifetime.

Arena/lifetime separation note:

- Flow runtime owns its own arena (independent from `NodeArena` / `StateArena`).
- Node teardown does not kill runtime by itself; owner teardown does.

## Error model

Use two-stage handling with explicit handled/unhandled result.

```cpp
enum ErrorHandleResult
{
  ERROR_UNHANDLED = 0,
  ERROR_HANDLED = 1
};
```

Typed local form:

```cpp
template <typename E>
struct ErrorHandleResultT
{
  enum Value { UNHANDLED = 0, HANDLED = 1 };
  Value value;
  E error;
};
```

Envelope form for parent bubbling:

```cpp
struct ErrorEnvelope
{
  int kind;
  int code;
};
```

Order:

1. local handler (`onFailure`)
2. if unhandled, bubble to parent flow `onFailure`

This allows local recovery while preserving hierarchical error handling.

Handler semantics:

- `onFailure` is non-consuming/pass-through for the flow value path.
- Multiple `onFailure` clauses are allowed in sequence.
- Matching is first-match-wins.
- `Handled` stops error bubbling.
- `Unhandled` continues bubbling to parent flow.

### Parent/child flow propagation

- Errors are delivered to the nearest `onFailure`.
- `Handled` stops propagation.
- `Unhandled` automatically bubbles to the parent flow.
- Parent/child flow wiring should use explicit `inState/outState` bindings.

### Matching strategy for `onFailure`

`onFailure` matching should stay simple and deterministic:

1. equality (`==`)
2. `Pattern::match(err)`
3. custom functor matcher

Use first-match-wins ordering.

Functor matcher example:

```cpp
onFailure(Handler::is5xxError, &handleServerError);
```

Default-clause shorthand:

```cpp
onFailure(&showErrorDialog); // same as onFailure(Handler::always, &showErrorDialog)
```

Jump-on-handle form:

```cpp
onFailure(Handler::is5xxError, &handleServerError, STEP_RETRY);
onFailure(&showErrorDialog, STEP_TERMINATE); // default + jump
```

Jump rule (v1):

- Jump destination is applied only when the matched handler returns `Handled`.
- `Unhandled` never triggers a jump and continues bubbling.
- Jump lookup uses the same per-flow `StepId` table used by `resume(StepId)`.

Success-jump form (v1):

```cpp
onSuccess(&cacheResult, STEP_NEXT);
```

Success-jump rule (v1):

- `onSuccess(..., STEP_X)` requests a jump after success observers run.
- Matching remains declaration-order; first jump request in the chain wins.
- `onSuccess(..., STEP_X)` uses the same per-flow `StepId` lookup as failure-jump/resume.

Parent-side matching:

1. fast path: `kind/code`

## Loading model

Use optional output state bindings:

- `trackLoading(MutableState<bool>*)` (nullable)
- `onSuccess(MutableState<T>*)` (nullable)
- `onError(MutableState<E>*)` (nullable)

v1 loading contract:

- set `isLoading = true` when flow starts
- keep `isLoading = true` while active or paused in `PENDING`
- set `isLoading = false` in terminal cleanup

`onSuccess` is not required for correctness; flows may terminate without updating a user-visible `result` state.

Structural release guarantees:

1. normal step completion path
2. terminal-state internal cleanup
3. runtime destructor internal cleanup fallback

Projection:

- single loading state: `true` from start until flow terminal cleanup, then `false`
- multi-target states are out of v1 scope

Cleanup API note:

- cleanup is internal runtime behavior
- user code should not call `releaseAll()` directly

## Handler pass-through semantics

`onSuccess` / `onFailure` are pass-through operators.

- They do not consume the step output value.
- They may appear multiple times in one scope.
- `onSuccess` fan-outs are evaluated top-to-bottom on the same value.
- Downstream steps continue with the current step context/output.

Attachment rule:

- `onSuccess`, `onFailure`, `onFinally` attach to the nearest `Step` scope.
- If no `Step` scope exists, they attach to the `Flow` scope.
- `onFinally` is argument-less in all scopes.
- If terminal details are needed, handlers should read owner-held states directly.

`onFinally` execution rule (v1):

- Step-scoped `onFinally` runs when that step is finalized (success/failure/cancel at step boundary).
- Flow-scoped `onFinally` runs once at flow terminal.
- A step may declare at most one `onFinally`; flow may declare at most one flow-level `onFinally`.
- If step-scoped `onFinally` proves too costly in v1 implementation, fallback to flow-terminal-only mode is permitted and must be documented at build time.
- `PENDING` is not terminal; step/flow `onFinally` does not run on `PENDING`.

Execution order rule (v1, explicit):

1. Active step is single (no parallel step execution in v1), so handlers are never interleaved across steps.
2. Inside one step scope, handlers run in declaration order (top-to-bottom).
3. Step-scoped `onSuccess`/`onFailure` run first for that step event.
4. Step-scoped `onFinally` runs after step-scoped success/failure handling for that step.
5. Flow-scoped `onSuccess`/`onFailure` run after step-scoped handlers for the same event type.
6. Flow-scoped `onFinally` runs last, once, at full flow terminal.

Failure bubbling note:

- `onFailure` matching remains first-match-wins.
- If step failure is `Handled`, parent bubbling stops.
- If `Unhandled`, it propagates to parent/flow `onFailure` chain.

Example:

```cpp
Flow()
  | Step(FLOW_STEP_READ, ReadFileAdapter())
      .input(&fileState)
      .onSuccess(&rawDataCache)
  | Step(FLOW_STEP_DECODE, DecodeImageAdapter())
      .onSuccess(bindToUI(&imageView))
      .onFailure(Handler::isOOM, &handleOOM)
      .onFailure(&showErrorDialog, STEP_TERMINATE);
```

Step-scoped `onFinally` example:

```cpp
Flow()
  | Step(FLOW_STEP_READ, ReadFileAdapter())
      .input(&fileState)
      .onFinally(&cleanupReadStep)
  | Step(FLOW_STEP_DECODE, DecodeImageAdapter())
      .onFinally(&cleanupDecodeStep);
// each onFinally runs when its step finalizes
```

## Resume semantics

Resume is opt-in per step:

- step with ID: resumable
- step without ID: not resumable

Prefer typed resume IDs over raw integers:

- `ResumeStep<StepId>(id)`

Example intent:

```cpp
onFailure(
  Error::NotFound,
  MyErrorHandler::showAsDialog,
  ResumeStep<FlowStepId>(FLOW_STEP_FILE_TO_BLOB));
```

If the handler returns/raises an `EmitterState`, the flow resumes from the specified step when emitted.

Control-flow API note:

- Primary jump style is handler-attached destination (`onSuccess/onFailure(..., STEP_X)`).
- v1 does not provide standalone `.goto(...)`.

## Timeout and retry

First iteration API:

- `timeoutMs(unsigned long)`
- `retryCount(int)`

`timeoutMs` targets the nearest step/scope on its left side.
Timeout failure enters the same local `onFailure` chain for that step/scope.

Shorthand form is allowed:

- `timeoutMs(ms, handler, resumeStep)`

This is syntax sugar for:

1. `timeoutMs(ms)`
2. `onFailure(timeoutPattern, handler, resumeStep)`

Policy details (backoff, jitter) are deferred.

## Proposed minimal user syntax

```cpp
Flow()
  | Step(FLOW_STEP_FILE_TO_BLOB, FileToBlobAdapter())
      .input(&fileState)
      .onSuccess(&binaryCache)
      .timeoutMs(30000)
      .onFailure(&handleBlobFailure)
  | Step(FLOW_STEP_BLOB_TO_IMAGE, BlobToImageAdapter())
      .onSuccess(&imageState)   // optional
      .trackLoading(&isLoading)
      .onFailure(&handleImageFailure);
```

Realtime transcoder-style step example:

```cpp
Flow()
  | Step(FLOW_STEP_OPEN_INPUT, OpenInputStreamAdapter())
      .input(&inputState)
  | Step(FLOW_STEP_DECODE_FRAME, DecodeFrameAdapter())
  | Step(FLOW_STEP_ENCODE_FRAME, EncodeFrameAdapter())
  | Step(FLOW_STEP_WRITE_OUTPUT, WriteOutputChunkAdapter())
      .onFailure(&handleTranscodeError)
      .onFinally(&cleanupTranscodeStep);
```

This keeps async I/O + decode/encode + write in one finite Step-based model.

Generic conversion-oriented style:

```cpp
Flow()
  | Step(FLOW_STEP_FILE_TO_BLOB, FileToBlobAdapter())
      .input(&fileState)    // File -> Blob
  | Step(FLOW_STEP_BLOB_TO_IMAGE, BlobToImageAdapter());  // Blob -> Image
```

## HTTP/response modeling

For network flows, prefer a single typed response payload:

- `Response<T>` with `body/status/headers/cookies/meta`

Example:

```cpp
Flow()
  | Step(FLOW_STEP_HTTP_TO_RESPONSE, HttpGetToResponseBlobAdapter())
      .input(&requestState)
      .onSuccess(&responseState)
      .timeoutMs(30 * 1000UL, MyErrorHandler::showAsDialog, ResumeStep<FlowStepId>(FLOW_STEP_HTTP_TO_RESPONSE))
  | Step(FLOW_STEP_RESPONSE_TO_IMAGE, ResponseBodyToImageAdapter())
      .onFailure(&handleDecodeFailure);
```

This is preferred over ad-hoc multi-slot success payloads.

## Retry policy example (401 refresh)

Keep retry counters internal to flow state for testability.

Reference scenario:

1. request returns `401`
2. refresh token step runs
3. original request retries once
4. if still `401`, emit `Unhandled`

Expected tests:

- refresh runs only on first `401`
- retry count is exactly one
- second `401` bubbles as unhandled
- loading state is released on all paths

## C++98 constraints

- no lambdas
- no `std::function` requirement in public API
- function pointer + functor object adapters are allowed
- no exceptions; use return values and explicit error states

## Static analysis note

Because step transitions are explicit in DSL nodes, AST-level inspection can enumerate:

- reachable steps
- terminal paths
- default/error fallthrough paths

This enables static validation of missing handlers and unreachable transitions.

## Integration direction

1. Implement only for SimpleViewer image load flow first.
2. Validate behavior and ergonomics with real UI/state updates.
3. Validate non-image bounded pipeline cases (transcoder/encoder-style steps).
4. Generalize to reusable Flow DSL after validation.

## Execution model scope (v1)

- No `cold/hot` distinction in v1.
- Flow execution is explicit and state-driven.
- Re-run happens only through explicit state transitions/resume paths.

## Runtime representation notes (v1)

- Type-safe chain construction reuses existing Loka DSL/template patterns already proven in Toolbox paths.
- Runtime execution keeps a lightweight step list in arena-owned storage.
- `resume(StepId)` uses a simple `StepId -> step index` lookup.
- v1 permits linear lookup because step counts are expected to be small.
- If needed later, lookup can be upgraded to sorted/binary-search or table mapping without changing DSL surface.

## Open questions

1. (none for v1 core)

## Resolved decisions (current)

- `Flow` is state-chain focused; external coupling is explicit via optional state bindings.
- `onFailureGlobal` is removed; `Unhandled` bubbles to parent `onFailure`.
- `Step(...)` boundaries are explicit in the `|` chain and do not require `endStep()`.
- `StepId` is opt-in; only ID-marked steps are resumable.
- Adapter run is tri-state (`PENDING/SUCCEEDED/FAILED`), not bool.
- `timeoutMs(ms, handler, resumeStep)` is shorthand for timeout + local `onFailure`.
- Matching order in `onFailure`: equality (`==`) -> `Pattern::match` -> custom functor (first-match-wins).
- `onSuccess` is optional; a flow may finish without writing a user-visible result state.
- handler-attached jump destinations (`..., STEP_X`) are the primary control-flow mechanism.
- `resume(StepId)` is per-flow only in v1 (no global resume registry).
- For cross-flow bubbling, use typed local errors internally and convert to lightweight `ErrorEnvelope(kind, code)` at boundaries.
- `trackLoading` in v1 is simple bool lifecycle (`start=true`, `terminal=false`) with terminal/destructor cleanup fallback.
- During `PENDING`, `trackLoading` remains `true`.
- `STEP_CANCELED` is a standard terminal destination for user-initiated or owner-driven cancellation.
- `timeoutMs(ms, handler, resumeStep)` requires an active `PlatformContext` or timer system passed at start.
- `EmitterState`-driven resume is out of scope for v1 and deferred to v1.1+.

## Converged summary (implementation target)

- `Flow` is a pass-through state chain; external side effects are explicit and optional.
- `Step(...)` boundaries are explicit in the `|` chain and do not require `endStep()`.
- `StepId` is opt-in; only ID-marked steps are resume/goto targets.
- `resume(StepId)` is scoped to a concrete flow runtime (per-flow).
- `onSuccess`/`onFailure` are non-consuming pass-through operators and may be declared multiple times.
- `onFailure` uses first-match-wins; `Handled` stops, `Unhandled` bubbles to parent flow.
- `timeoutMs(ms, handler, resumeStep)` is shorthand for timeout + local failure handling; requires a platform timer hook.
- handler-attached jump destinations (`..., STEP_X`) are the control-flow mechanism in v1.
- `cancel` is modeled as terminal step transition (`STEP_CANCELED`).
- `onFinally` is step-scoped by default and argument-less; flow-level `onFinally` remains available for terminal-only cleanup.
- `onFinally` does not run on `PENDING`; it runs only on terminal finalization.
- Handler execution order is deterministic: step scope (top-to-bottom) first, flow scope after, and flow `onFinally` last.
- v1 omits `Conditional`; branch routing is expressed by explicit steps and handler-driven step jumps.
- Local flow errors remain typed; parent bubbling crosses lightweight `ErrorEnvelope(kind, code)` boundary.
- Flow terminates explicitly (handler destination `STEP_TERMINATE`) or implicitly (last step completion), then internal cleanup runs automatically.
- Flow runtime memory is arena-owned and bulk-freed at teardown.
- Loading exposure for v1 is UI-facing `bool` with simple lifecycle semantics.
- `onFinally` is argument-less by design; handlers inspect owner-held states when needed.
- 401 refresh/retry policy should be modeled as internal flow state and verified by tests.
- `EmitterState`-driven resume is deferred until after explicit `flow->resume(id)` behavior is validated.
- `Step(id, adapter)` is the single composition primitive for bounded transform/async stages that are expected to terminate.
- Long-lived resource holders (for example always-on players) should live outside v1 Flow runtime ownership.
- Runtime result model is tri-state (`FLOW_RUN_PENDING/SUCCEEDED/FAILED`) to support explicit pause/resume semantics.

### v2 direction (kept as future note)

- Prefer flow composition (`call/join/race`) over introducing a separate parallel DSL root.
- Keep external API simple by aggregating join/race outcomes into explicit `outState`.
- Introduce token/list-based loading reference tracking when parallel/join/race execution is added.
