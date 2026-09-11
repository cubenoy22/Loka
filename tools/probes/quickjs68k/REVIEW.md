# Shape review

## Gate 1

- Scope: standalone engine measurement; no Scene replacement, UI DSL, or core changes. Issue #657 is not a dependency.
- Reuse the existing pinned four-source engine target, not a second dependency definition. No feature stripping before measurement.
- The host owns runtime, context, and values in nested local lifetimes. Failures stop the probe and release completed resources.
- Measurement allocation accounting belongs to the probe allocator only; no framework state or production introspection API.
- Cost: one engine/context construction, one synchronous evaluation, one teardown. Allocator accounting is O(1) per allocation; no foreign rows are walked.
- Prefer a native allocator-size query over per-allocation headers where Classic supports it. Separate requested engine memory from process/partition occupancy and static tables.

## Gate 2

- Added build doors: standalone CMake project, reuse of the existing engine target,
  build-local source preparation. They run once per configure/build; no production
  build graph changes. Source preparation copies dependency-owned files only.
- Added host door: `main`, analogous to a bounded console test, constructs one
  runtime/context, evaluates once, and releases values/context/runtime in order.
  No native UI callbacks, deferred work, retained ledger, or Scene handoff exists.
- Added allocator doors: allocate/calloc/free/realloc/usable and runtime creation;
  these implement QuickJS's existing allocator interface, not a new framework API.
  Allocation callbacks read only their own pointer and perform O(1) accounting;
  realloc copies O(min(old,new)) bytes owned by that allocation. Native Memory
  Manager costs are unmeasured. No foreign registry walk is introduced.
- Added measurement fields, individually: `allocation.live` is written by allocator
  allocation/release and read inside the same allocator module; `allocation.peak`
  is written/reset by that module's allocation/runtime lifecycle. Both are facts
  owned by the new probe allocator, not fields bolted onto a framework owner.
  Reporting is through `probe_memory_report`, not foreign field access. Live
  bytes could only be recomputed by adding an allocation ledger; historical peak
  cannot be derived from current live bytes. No extra flags or per-block headers.
- Added timer door: `loka_probe_hrtime_ns`, called by QuickJS's existing clock
  surface, O(1) Toolbox query. No cached phase/clock identity. Runtime correctness
  on the target remains pending; no claims about older machines without this trap.
- Added source substitutions: three cutils condition/clock changes and four local
  type fixes; exact match refusal handles a changed upstream pin. No warning is
  downgraded, no upstream cache is edited, no language feature is removed.
- Added SIZE resource: explicit measurement partition; not a support assertion.
- Refusal paths: runtime/context refusal still prints final FAIL and cleans up
  completed owners; realloc refusal preserves original payload and accounting.
  The runtime-singleton assert disappearing does not create an app-facing hole:
  this closed host calls creation exactly once. It is not a reusable library API.
- Validation: four-source 68K library and application build/link; Linux evaluation
  PASS; source/style/whitespace checks. Classic allocator and clock are only
  build-verified; their runtime discrimination is explicitly handed to the human.
- Deliberately pending: actual high-water values, partition fit, target evaluation,
  release-to-zero, and performance. No fabricated memory figures or compatibility
  claim. The probe's copy-based realloc overhead is reported, not generalized to
  a future production allocator. No pre-PR delegation: no PR opened in this task.
