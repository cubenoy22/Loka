# Contributing to Loka

Thank you for contributing.

## Scope and Compatibility

- Library/core compatibility targets macOS Tiger through Snow Leopard.
- Consumer apps are expected to run on Big Sur and newer.
- Any feature PR must clearly state the supported OS range.

## Required PR Information

Every PR must include:

- Change summary.
- Supported OS range for the change (for example: `10.4-10.6`, `10.7+`, `11+`).
- Verification type for each claimed platform: `build-verified` (compile/link passed) or `runtime-verified` (app launched and behavior checked).

## Testing Requirements

For logic-heavy changes:

- Add or update unit tests.
- Maintain at least 90% coverage for the affected logic.

For UI changes:

- Implement an autopilot/test-driving path for the changed flow (timer/state/event-driven is acceptable).
- Attach visual evidence (capture or recording) showing the expected result.

## Objective-C Rules (Library/Core)

- Library/core Objective-C(++) code is non-ARC.
- `@property` / `@synthesize` are allowed.
- Explicit ownership semantics are required (`retain`, `assign`, or `copy`).
- Direct ivar access (`obj->ivar`) is forbidden.
- Internal state access must go through private getter/setter methods.

## Build Entry Points (macOS)

- `scripts/macos/build-10_4.sh`: Tiger/Leopard compatibility path.
- `scripts/macos/build-10_7.sh`: Lion and newer path.
- `scripts/macos/build.sh`: shared internal driver.

## Retro68 Toolchain Location

The Retro68 toolchain is resolved in this order (cmake/toolchains/Retro68.cmake):

1. `RETRO68_TOOLCHAIN_DIR` cache variable or environment variable
2. Auto-detection under `$HOME/Retro68-build` and `$HOME/Retro68`

If your toolchain lives elsewhere, do **not** copy or edit the repository
presets. Put a local preset in `CMakeUserPresets.json` (gitignored, picked up
automatically by CMake and VS Code) that **inherits** a repository preset and
overrides only the location:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "retro68-68k-local",
      "inherits": "retro68-68k-release",
      "cacheVariables": {
        "RETRO68_TOOLCHAIN_DIR": "/path/to/Retro68-build/toolchain/m68k-apple-macos/cmake"
      }
    }
  ]
}
```

Inheriting matters: the repository presets carry the Classic size/flag policy
(`-Os`, `-fno-exceptions`, `--gc-sections`, the compact/diagnostic split — see
#135/#136/#137). A standalone local preset silently loses them and Classic
binaries grow back by roughly 40%.

## Commit and Review Hygiene

- Keep commits small and scoped.
- Do not amend commits except immediate tiny follow-up fixes.
- Prefer compile-time guarantees over runtime checks where practical.
