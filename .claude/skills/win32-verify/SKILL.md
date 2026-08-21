---
name: win32-verify
description: Build and verify Loka on a local Win32 machine from WSL — vcvarsall via bat file, worktree setup, test evidence rules, assert-dialog hang workaround, screenshot capture. Use when a change touches win32/ or needs real-machine Win32 verification.
---

# Win32 real-machine build & verify (WSL → MSVC)

Hosted CI covers x64 Debug only; the native architecture and full-width-path
behavior are verified on a real machine here (#172).

Never hardcode the Visual Studio path or the target architecture. Both differ
per rig, the install path does not follow the product year (VS 2026 lives under
`\18\`), and a wrong guess fails at `call`. Discover both in the bat file — see
Build below.

## Checkout layout

- **The WSL ext4 clone is the source of truth.** Clone it with WSL `git`, not
  Git for Windows: Git for Windows checks out CRLF, WSL `git` has
  `core.autocrlf` unset, and every tracked file then reads as modified.
- **Work in a worktree on `/mnt/c`** (NTFS). Windows tools cannot see the ext4
  clone; create the worktree from it with `git worktree add`.
- **Silence the `/mnt/c` filemode noise once per worktree.** drvfs reports every
  file as 0777, so `git status` shows a `100644 => 100755` mode change for the
  whole tree. `core.filemode` is shared across worktrees, so set it per
  worktree instead of turning it off in the ext4 clone:

  ```sh
  git -C <ext4-clone> config extensions.worktreeConfig true   # once per clone
  cd <mnt-c-worktree>
  git config --worktree core.filemode false
  ```

- **Never pass complex arguments to `cmd.exe` directly** — WSL quoting mangles
  them. Write a `.bat` file in the worktree and run
  `cmd.exe /c <name>.bat > log.txt 2>&1` from that directory.
- **Full output to a file, never `| tail`** — piping hides progress and
  truncates the execution evidence.
- Delete the worktree when the PR merges. The bat files and the
  `golden.lrpk` the test binary writes from the worktree root are
  gitignored, so they cannot reach a commit if you forget.

## Build

`build.bat` (the two builds are both required — neither implies the other,
see `.github/workflows/ci.yml`). The first two lines resolve the toolchain, so
the same file works on any rig:

```bat
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set VSPATH=%%i
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (set VCARCH=arm64) else (set VCARCH=x64)
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" %VCARCH% || exit /b 1
cmake --preset win32-debug || exit /b 1
cmake --build --preset win32-debug || exit /b 1
cmake --build --preset win32-tests || exit /b 1
```

The presets are architecture-neutral — they use whatever `cl.exe` and
`ninja.exe` vcvarsall puts on `PATH`, so nothing below the `call` is per-rig.

## Test evidence rules

- Run the suite with the exit code captured, by its generated path (the CI
  workflow's invocation; a bare `LokaTestsWin32.exe` from the worktree root
  finds nothing):
  `build\win32\Debug\example\HelloWorld\LokaTestsWin32.exe > test-log.txt 2>&1`
  then `echo TESTEXIT=%ERRORLEVEL%`.
- **Evidence for "test N exists and runs" is `--list` plus a single-test
  run**, never a grep of the output banner (banners are each test's own
  printf and prove nothing).
- Full-width path coverage: single-run all three and check `EXIT=0`. They cover
  different seams, so naming a subset understates what a change needs.
  - `testWin32OpenWriteTruncateAcceptsFullWidthPath` — the write seam. It has no
    ACP gate at all, so it is positive coverage on any rig.
  - `testWin32OpenReadAcceptsFullWidthPath` — the read seam, plus a contrast pin
    that a narrow `fopen` still cannot reach the file.
  - `testWin32FileFromWidePathSurvivesToOpen` — the producer seam
    (`win32::FileFromWidePath`), which is where #15 was actually lost. **A change
    to a `W` entry point or to the open-file dialog needs this one**; the other
    two open a path we built ourselves and can pass without exercising it.
  - The two contrast pins skip only on a UTF-8 ACP (`GetACP() != CP_UTF8`,
    `tests/Win32FilePathTests.cpp:103,169`), so **any non-UTF-8 ACP
    discriminates** — 932 is one rig's value, not the requirement. Confirm the
    codepage rather than assuming it: on a UTF-8 ACP the pins print `[skip]` and
    the run proves less than the exit code suggests.

## Assert red → dialog hang (#210)

A failing `assert` in an MSVC Debug build raises an abort dialog and hangs
forever. To capture a red run:

1. Redirect stderr to a log **on the Windows side** (inside the bat).
2. Run under WSL `timeout`, e.g. `timeout 60 cmd.exe /c run-tests.bat`.
3. Reap with `taskkill.exe /IM LokaTestsWin32.exe /F`.
4. Read the assert message (`Assertion failed: ... file:line`) from the log.

This is also the mutation-testing loop: mutate → red assert captured from the
log → restore → green (precedent: #211, `plans/199-mutation-log.md`).

## Screenshots (visual verification)

Capture with a PowerShell script (`Add-Type` + `System.Drawing`/GDI screen
copy, or the repo's `scripts/mame-run.ps1` style as a reference), inspect the
PNG by Reading it. For PR embedding, push assets to the `pr-assets` branch and
link raw URLs (example: the comment thread on #162).
