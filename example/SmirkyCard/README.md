# SmirkyCard runtime experiment

This optional example has two **C++-defined Scenes**. Each card has an editable
script box; press **Run** to evaluate it with QuickJS and show its string result
or exception. **Run JavaScript** remains the navigation experiment: it evaluates
the displayed expression, returns `"first"` or `"second"`, and C++ installs
that card in the same Window. Each visit creates a fresh Scene; the old Scene is
retired through SceneManager.

## MAIN.JS cards

`MAIN.JS` beside the application replaces the built-in card definitions at
launch. On Windows it lives beside the executable; on macOS it is a bundle
Resource; on Classic it is a plain data-fork file beside the application. The
repository's `MAIN.JS` is the built-in sample, so it is a useful starting point.
Edit the file on the disk and launch again to see changed cards. While the app
is running, press **Reload MAIN.JS** on Card One to re-read it and rebuild the
current card by name. Reload releases the previous script generation after its
outgoing card is reclaimed; card fields, counters, and other state are not
carried over. A reload failure stays on the current card and appears in its
status text. A card that throws while it is constructed or composed shows an
error with its own **Reload MAIN.JS** button, so fixing the file does not require
relaunching. Stage Classic with:

```sh
scripts/mame-dev-disk.sh build/retro68/68k/Release/example/SmirkyCard/LokaSmirkyCard68K.bin example/SmirkyCard/MAIN.JS example/SmirkyCard/MINES.JS
```

`mame-boot-disk.sh` takes the same application binary followed by `MAIN.JS` and
`MINES.JS` as plain-data arguments.

`open(name)` loads a sibling script into a fresh engine and shows its `first`
card. MAIN.JS links to `./MINES.JS`; the placeholder there links back to
`./MAIN.JS`. Names must be flat filenames: one leading `./` is stripped, but
paths (including `../`), backslashes, colons, and embedded NUL are refused.
Each open reads the disk again. `reload()` rereads the current engine's file
and keeps the current card id; built-in cards retry MAIN.JS. Both operations
limit source files to 64 KiB and preserve the live card and engine on read,
evaluation, interruption, or missing-entry failure, reporting the filename in
the status. Constructor/compose refusal after admission uses the existing
native reload button (currently labelled **Reload MAIN.JS** even for a sibling).
No card-local state survives a successful open or reload.

The AppConfig owns one runtime/context and outlives all windows. Card boundaries
borrow it. The interpreter returns a card identifier and releases its result
before navigation; there are no JS-held Node pointers or JS callbacks. A small
C seam keeps QuickJS's modern header macros out of the C++98 application.

`CardScene::replaceWith` adopts the replacement through SceneManager. The Window
root seat mounts and composes it at the next App admission, then installs it on
the existing controller. A refused preparation preserves the old card and leaves
the replacement pending for the next admission. The outgoing Scene is reclaimed
at the following admission. Evaluation/definition allocation errors likewise
leave the old card installed; native projection failures after installation do
not have a rollback protocol.

## Build

The example requires CMake 3.18 or later. It is off by default, so ordinary Loka
builds do not fetch or link QuickJS. Enabling it downloads the pinned QuickJS-ng
v0.16.2 source archive and verifies its SHA-256; only the engine is built.

From a configured Windows developer command prompt:

```sh
cmake --preset win32-debug -DLOKA_BUILD_SMIRKYCARD=ON
cmake --build --preset win32-debug --target LokaSmirkyCardWin32
```

On macOS:

```sh
cmake --preset macos-debug -DLOKA_BUILD_SMIRKYCARD=ON
cmake --build --preset macos-debug --target LokaSmirkyCardMacOS
```

For an existing dependency checkout or an offline build, also pass
`-DFETCHCONTENT_SOURCE_DIR_SMIRKYCARD_QUICKJS=/absolute/path/to/quickjs`.
Use commit `1ab8676f4b6d6d669baeb5f21790fb9734636a20` to reproduce the pinned
build; an override deliberately uses the caller's supplied source.

Linux/WSL provides a headless integration test, not a GUI target:

```sh
cmake -S . -B build/SmirkyCard-Testing -G Ninja \
  -DTEST_BUILD=ON -DLOKA_BUILD_SMIRKYCARD=ON -DLOKA_WARNINGS_AS_ERRORS=ON
cmake --build build/SmirkyCard-Testing --target LokaSmirkyCardTests
ctest --test-dir build/SmirkyCard-Testing -R '^smirkyCardSceneSwitch$' --output-on-failure
```

The test evaluates expressions, checks failure/interrupt recovery, and fires
the real Button binding for 20 alternating Scene replacements. It checks the
mounted title and deferred Scene retirement, then destroys the final Window.

## Verification of the initial experiment

- Windows x64 / MSVC: build-verified and runtime-verified. A native-button probe
  checked six alternating card transitions in the same Window, five child
  controls after each transition, and a clean application exit.
- Linux / GCC: the headless test passes with warnings as errors, and with
  AddressSanitizer, UndefinedBehaviorSanitizer, and leak detection enabled.
- macOS: a build target is provided; not yet build-verified or runtime-verified.
- XP, legacy Mac OS X, and Classic Mac: no compatibility claim from this run.

## Deliberate limits

- Synchronous global scripts only. No file watching, module loader, Promise job
  pump, or JS UI DSL.
- One runtime with an 8 MiB engine allocation limit, 256 KiB JS stack limit,
  and an evaluation-local interrupt budget. These are experiment limits, not
  measurements or a supported Classic memory profile.
- Window and Menu remain ordinary C++ definitions.
- Classic builds refuse this option until macQJS/Retro68 integration is added.
  This dependency selection does not establish XP or legacy Mac OS X support.
- QuickJS-ng is MIT-licensed; its fetched source includes the upstream LICENSE.

Upstream: [QuickJS-ng](https://github.com/quickjs-ng/quickjs),
[macQJS Classic port](https://github.com/mplsllc/macQJS).

## RetroPPC QuickJS stack allocation

RetroPPC GCC 16.1.0 can place fixed locals at frame offset 72 while returning
an ordinary `alloca` pointer at offset 80 after the dynamic stack adjustment.
When the requested allocation is a multiple of 16 bytes, its last eight bytes
overlap those locals. In the second card's compose call, the
closure-reference array overlapped the QuickJS frame; `get_var_ref` then treated the
string tag `0xfffffff9` as a pointer and raised a PPC data-access exception.
The local MAME reproduction used pmac6100, 72 MiB RAM, and J1-8.1.

The Classic source preparation routes QuickJS's five stack allocations through
`ClassicQuickjsStack.h`. On RetroPPC it reserves one additional 16-byte ABI
stack unit, and the existing stack-limit check accounts for the same padding.
The 68K path keeps its original allocation size. Ordinary `alloca` is retained
because some argument buffers outlive the block allocating them; an aligned
builtin with block lifetime would not preserve that contract.

Run the compiler-layout pin against the configured PPC build:

```sh
python3 tests/scripts/ClassicQuickjsStackTest.py --build-dir build/retro68/ppc/Release
```

The pin uses the actual QuickJS compiler command and an ordinary-alloca
control. With GCC 16.1.0 the control overlaps by eight bytes; the patched
payload ends at offset 64, before fixed locals at 72. An unfamiliar assembly
shape fails for inspection. This compile check does not replace the Classic
runtime check: evaluate `1+1`, switch to Card Two, evaluate `2+2`, switch back,
and repeat before checking Reload.

The 2026-09-20 fix was build-verified for PPC and 68K with Retro68 GCC 16.1.0.
After removing diagnostic logging, it was runtime-verified on MAME 0.289
pmac6100 (72 MiB, J1-8.1): `1+1` and `2+2` evaluation, ten Card One/Card Two
round trips (20 navigation calls), and Reload. This evidence does not cover
physical hardware.

## JS seats and clickables

Declare `state(initial)` only in a card constructor. Initial values may be
Strings, integers, or Bools; the seat keeps its type. `Text(seat)` formats
integers and Bools, and `EditText(seat)` requires a String seat.

`Button(textOrSeat, handler)` and `Cell(textOrSeat, handler)` accept a literal
String or a live String seat. The latter renders Loka's native clickable text
Cell, as used by the C++ MineSweeper. A handler runs with the card as `this`;
seat writes update the label without composing the card again.
Only Button has `.enabled(boolSeat)`. Both support `.TEST_ID('name')`.

`Grid(rows, cols, children)` lowers to Loka's native Grid, with equal-sized
cells in row-major order. Both dimensions must be numeric integers in 1..16.
The third argument accepts a tree node or an array of nodes; like stacks,
only one array level is flattened. The child count must equal `rows * cols`
(up to 256); a mismatch refuses with `Grid requires exactly rows * cols
children (N)`. Nested arrays are refused. Grid has its own child limit;
Row/VStack remain limited to 16. See [the board example](../../docs/smirkycard/clickables.md#grids).

Each card has two independent admission budgets, declared in
[src/CardRecords.hpp](src/CardRecords.hpp): `kCardSeatBudget` (128 seats) and
`kCardClickableBudget` (128 total Buttons plus Cells). Exceeding either shows
the existing refusal card with a message naming that budget and the Reload
button. Records are allocated only for seats declared and clickables lowered;
there is no eight-slot allocation or dispatch table. The refusal UI's Reload
button is a separate host control. The existing 16-children-per-stack and
depth-eight tree limits still apply. Grid capacity does not raise the seat or
clickable budgets (a 256-cell Grid can use Text, but cannot contain 256 Cells).

The card owns stable seat handles and JS handler records. Each clickable is a
small child Component that owns its emitter and binds in its own declaration
window. Detach withdraws that binding synchronously; the card releases records
only during its destruction after its subtree is detached. Child destruction
never accesses the borrowed handler.

## Card scenarios (TEST_BUILD, stage 1)

A runner calls `ScriptRuntime::enableRunner(sink, clock, seed, companionName, bakedText)`
before loading sources or creating cards. It owns the sink and clock until all
cards are gone. The optional nonempty baked text replaces companion file reads,
including reload/open; omitting it retains the file-based runner. Without this
opt-in, `Flow`, `scenario`, `c.run`, and `c.test`
are absent. Their implementation and storage are excluded from non-TEST_BUILD
builds.

The companion file registers `scenario('first', function(c) { c.run(chain); })`.
Both sources are evaluated in one candidate engine before commit; registration
and file errors are setup failures. `loadMain` reports them through
`mainErrorFor`, and `prepareReload`/`prepareOpen` return a null candidate with an
error. A missing main source may select the built-in source, but its companion
must still load and register successfully. An invalid main source never silently
falls back under the runner.

`Flow().step(fn).named(name).onSuccess(fn).onFailure(fn)` describes a chain.
`c.run` accepts it once, only during that card's scenario invocation, and closes
its mutation door. Each function receives the preceding return value (initially
`undefined`). Each authored action uses `RunOnce`, with an automatic `Settle`
afterward; all C++ edges remain `Scene*`. JavaScript values and functions remain
rooted in their originating engine. `waitUntil`, `options`, retry, and resume are
not part of stage 1.

After mounting, the runner advances its clock and calls
`CardScene::tickScenario()` once per tick. This typed door reaches the root card
without a tree search. The scenario is invoked on the first safe tick, after
children exist, never during attach. It runs once per candidate engine's initial
Scene: reattaching or navigating with `go` does not invoke it again. Reload/open
validate fresh sources and provide a fresh scenario invocation. Scene admission
and reclamation happen outside the tick; synchronous detach cancels work already
on the stack, whose step outcome and canceled terminal are recorded on unwind.
No JavaScript terminal callback runs after Detaching.

The immediate test operations are:

- `c.test.click(id)`: Button or Cell, with a flush; a disabled Button fails.
- `c.test.enabled(id)`: Button enabled state; Cells return true.
- `c.test.text(id)`: Text, Markup's concatenated text segments, Button/Cell label, or
  EditText contents. IDs must identify exactly one node of the requested kind.
- `c.test.log(text)`: one percent-escaped audit record (`log text=...`).
- `c.test.random()`: a masked 32-bit LCG, with an integer-constructed binary64
  result exactly equal to `state / 2^32`. Every card Scene starts from the runner
  seed, before its factory executes; `go` also creates a fresh random stream.

The host runner harness is in [SmirkyCardTests.cpp](tests/SmirkyCardTests.cpp);
`LokaSmirkyCardTests --scenarios` runs these mechanism checks. The generic Classic
standalone runner bakes both sources through
`smirkycard_add_scenario_runner` in [CMakeLists.txt](CMakeLists.txt). Add content
with another call specifying CARD, FLOW, SCENARIO, APP_NAME and SEED; no C++
changes are needed. In any Classic build with `LOKA_BUILD_SMIRKYCARD` enabled,
the first call registers `LokaSmirkyCardStandaloneFlow68K_APPL` or
`LokaSmirkyCardStandaloneFlowPPC_APPL` (excluded from ALL); the VS Code task
"Build: Retro68 68K SmirkyCard" builds it together with the app. It uses SmirkyCard's SIZE resource, writes
LOG.TXT beside the application, logs the seed, advances the clock once per 0.1 s
idle callback, and quits after the terminal audit record.

[MINES.FLOW.JS](MINES.FLOW.JS) plays one board: flags a cell, returns to reveal
mode and checks a zero-cell flood. MINES.JS uses `c.test.random` only under the
runner; its ordinary module-level generator and New Game behavior are unchanged.
`LokaSmirkyCardTests --mines-scenario` checks the real files and the baked app
configuration, including the [expected audit](tests/MINES.audit).
The standalone audit is not registered in `scenarios.txt`: that file also controls
the approved MAME golden cells. Until its registration policy is decided, compare
LOG.TXT directly with the expected audit (`cmp`), rather than using
`verify-standalone-audit.sh`.
