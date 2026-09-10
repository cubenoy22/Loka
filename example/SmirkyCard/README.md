# SmirkyCard runtime experiment

This optional example has two **C++-defined Scenes**. Press **Run JavaScript**
on either card: QuickJS evaluates the displayed expression, returns `"first"`
or `"second"`, and C++ installs that card in the same Window. Each visit creates
a fresh Scene; the old Scene is retired through SceneManager.

The AppConfig owns one runtime/context and outlives all windows. Card boundaries
borrow it. The interpreter returns a card identifier and releases its result
before navigation; there are no JS-held Node pointers or JS callbacks. A small
C seam keeps QuickJS's modern header macros out of the C++98 application.

`CardScene::replaceWith` mounts the replacement on the existing Window-owned
controller before SceneManager attaches it. SceneManager does not perform that
projection handoff by itself. Evaluation/definition allocation errors leave the
old card installed. This example does **not** add rollback for a failure during
the new Scene's native attachment.

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

- Fixed synchronous scripts, defined beside the cards in `src/CardNodes.hpp`.
  No editor, file watching, module loader, Promise job pump, or JS UI DSL yet.
- One runtime with an 8 MiB engine allocation limit, 256 KiB JS stack limit,
  and an evaluation-local interrupt budget. These are experiment limits, not
  measurements or a supported Classic memory profile.
- Window and Menu remain ordinary C++ definitions.
- Classic builds refuse this option until macQJS/Retro68 integration is added.
  This dependency selection does not establish XP or legacy Mac OS X support.
- QuickJS-ng is MIT-licensed; its fetched source includes the upstream LICENSE.

Upstream: [QuickJS-ng](https://github.com/quickjs-ng/quickjs),
[macQJS Classic port](https://github.com/mplsllc/macQJS).
