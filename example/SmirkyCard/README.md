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
scripts/mame-dev-disk.sh build/retro68/68k/Release/example/SmirkyCard/LokaSmirkyCard68K.bin example/SmirkyCard/MAIN.JS
```

`mame-boot-disk.sh` takes the same application binary followed by `MAIN.JS` as
a plain-data argument.

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
