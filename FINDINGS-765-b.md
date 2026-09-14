# GH #765 PR B findings

Assumptions: this branch starts at `a75609d8`, whose `DerivedNodeState<T>`
door is authoritative; no guide match means no Programming Guide edit is
needed. The existing shared HelloWorld responsive-test translation unit is
already registered in both host test mains, so no new test header or main
registration was needed.

## Per-file diff

| File | Lines | Change |
| --- | --- | --- |
| `example/HelloWorld/src/MainNode.hpp` | 27-53 | Replaces the three mutable output seats and six cache primitives with three `DerivedNodeState<String>` seats and private evaluator declarations. |
| `example/HelloWorld/src/MainNode.cpp` | 36-116 | Adds pure, private evaluator classes. BMI retains `CollectUtf8`, `strtod`, every non-positive guard, and `snprintf("BMI: %.2f", bmi)`. |
| `example/HelloWorld/src/MainNode.cpp` | 136-163 | Declares inputs first, assigns fruits, then registers all three derived seats; removes output watches and initial refreshes, preserving the layout watch. |
| `tests/HelloWorldResponsiveTests.hpp` | 4-7 | Declares the new shared host test. |
| `tests/HelloWorldResponsiveTests.cpp` | 128-171, 287-379 | Adds composed-node lookup and a host pin for startup, BMI transitions and rounding, fruit fallback, and probe/toggle behavior. |
| `tests/OwnershipDumpTests.cpp` | 743-750 | Updates the HelloWorld storage evidence: derived seats are tagged heap states, so the unchanged total is 11 states split as 8 arena / 3 heap. |

`docs/ProgrammingGuide.en.md` was searched for `refreshActionSummary`,
`CacheValid`, and `t.watch(this->heightInput_`; none matched, so no guide
snippet changed.

## Behavioral difference

The old BMI cache decided whether to call `set()` from
`int(bmi * 100.0 + 0.5)`. The derived seat correctly compares the displayed
formatted string. At a formatting boundary these are not equivalent: with
height `100` cm, changing weight from `20.6249` to `20.625` produces
`BMI: 20.62` for both values on the verified host, but the old integer buckets
are 2062 and 2063. The old cache would publish a redundant reset; the derived
state produces no observer callback. The host pin also changes weight to
`20.635`, which displays `BMI: 20.64` and produces exactly one callback.

## Evidence

| Check | Configuration | Result |
| --- | --- | --- |
| Native configure/build | `testing`, Debug, GNU 13.3, C++98, warnings-as-errors | Pass |
| Native tests | `ctest --preset testing -j 8` | 866/866 passed; one expected-red test skipped |
| Assert audit | `python3 tools/ci/check_test_asserts.py` | 431 files, 712 learned pairs, no findings |
| Whitespace | `git diff --check` | Pass |
| Retro68 configure/build | `retro68-68k-release`, m68k, warnings-as-errors | Pass: `LokaHello68K_APPL`, `LokaHelloWorldTestsToolbox68K_APPL` |
| Retro68 code artifact | baseline `a75609d8` vs this change | `LokaHello68K.code.bin`: 292096 -> 292608 bytes (+512); the requested shrink/hold target was not met. |

The 512-byte artifact increase is recorded rather than hidden. It comes with
the first shipping instantiation of the derived `String` machinery and three
evaluator virtuals in this app; no safe local reduction was found without
changing the requested three-evaluator/derived-seat shape.

MAME cells for the delegator: `helloworld/startup`,
`helloworld/toggle-action-probe`, and `helloworld/bmi-roundtrip`. Their golden
strings remain byte-identical.

## Shape-review lens walk

- Door/field duplication: the three `EvalFn` classes share only the pure
  `DerivedState<String>::EvalFn` protocol. A common base would add an empty
  abstraction while their dependencies and policies differ (two inputs,
  one input plus immutable vector, and parsing/formatting), so each evaluator
  remains the smallest meaningful owner of its references.
- Check absent in a configuration: both native and Retro68 compile the seats;
  the host behavioral observer pin is unavailable to the Toolbox scenario
  application, so the existing three MAME cells remain the native verification
  path.
- Step on one path only: inputs are registered once in the constructor and
  recomputation travels through the tracker's existing derived dependents
  walk; action methods now write only their source fact.
- Field never read: every evaluator reference is read by its `operator()`;
  each `DerivedNodeState` is read by composition through `.state()`. Deleted
  cache fields and refresh doors have no replacement bookkeeping.
- API shape for tests: the test observes the composed `TextNode` state and
  drives existing edit/popup/button ports; no production test accessor or
  test-only API was added.

Primitive-member inventory: net -6 in `MainNode` (`actionSummaryCacheValid_`,
`lastActionSummaryEnabled_`, `lastActionSummaryCount_`, `bmiCacheValid_`,
`lastBmiWasValid_`, `lastBmiHundredths_`); none added.

Cost line: zero per update beyond the tracker's existing dependents walk.

## Draft PR body

Title: HelloWorld keeps a hand-written summary cache (three primitive members) to derive a Text from a counter and a flag, which DerivedNodeState expresses directly (#765)

Mechanism: replace MainNode's three output caches with owner-seated
`DerivedNodeState<String>` values and pure evaluator objects that borrow only
their declared input seats (and the immutable fruit list where needed).

Change: action summary, fruit message, and BMI result now recompute through
their dependency edges. The old cache flags, last-value members, refresh
methods, binding watches, and action-side refresh calls are removed. BMI output
formatting and parsing stay byte-for-byte compatible. The ownership dump now
properly records the three derived states in tagged heap storage.

Tests: shared host coverage pins startup strings, invalid/valid/invalid BMI,
equal/different formatted rounding outputs and observer behavior, fruit
fallback, and toggle/probe behavior. Full native suite, assert audit,
whitespace check, and both requested Retro68 targets pass.

Goldens: no golden text changed. Delegator should run `helloworld/startup`,
`helloworld/toggle-action-probe`, and `helloworld/bmi-roundtrip` in MAME.

What remains: the Retro68 `LokaHello68K.code.bin` grows 512 bytes (292096 to
292608), contrary to the desired shrink-or-hold outcome; this needs a follow-up
size decision if that requirement is a release gate.
