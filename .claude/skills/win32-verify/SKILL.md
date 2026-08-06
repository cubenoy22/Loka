---
name: win32-verify
description: Build and verify Loka on the local Win32 ARM64 machine from WSL — vcvarsall via bat file, test evidence rules, assert-dialog hang workaround, screenshot capture. Use when a change touches win32/ or needs real-machine Win32 verification.
---

# Win32 real-machine build & verify (WSL → MSVC ARM64)

The local rig: Windows on ARM64, `ACP=932` (CP932), VS 2022 Community at
`C:\Program Files\Microsoft Visual Studio\2022\Community`. Hosted CI covers
x64 Debug only; ARM64 and full-width-path behavior are verified here (#172).

## Ground rules

- **Work in a worktree on `/mnt/c`** (NTFS), e.g.
  `/mnt/c/Users/cuben/source/repos/loka-<issue>`. Windows tools cannot see the
  WSL ext4 clone (`~/loka`); create the worktree from it, `git worktree add`.
- **Never pass complex arguments to `cmd.exe` directly** — WSL quoting mangles
  them. Write a `.bat` file in the worktree and run
  `cmd.exe /c <name>.bat > log.txt 2>&1` from that directory.
- **Full output to a file, never `| tail`** — piping hides progress and
  truncates the execution evidence.
- Delete the bat files and the worktree when the PR merges.

## Build

`build.bat` (the two builds are both required — neither implies the other,
see `.github/workflows/ci.yml`):

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" arm64
cmake --preset win32-debug || exit /b 1
cmake --build --preset win32-debug || exit /b 1
cmake --build --preset win32-tests || exit /b 1
```

## Test evidence rules

- Run the suite with the exit code captured:
  `LokaTestsWin32.exe > test-log.txt 2>&1` then `echo TESTEXIT=%ERRORLEVEL%`.
- **Evidence for "test N exists and runs" is `--list` plus a single-test
  run**, never a grep of the output banner (banners are each test's own
  printf and prove nothing).
- Full-width path coverage: single-run the tests that create files like
  `loka-Ａ.bin` and check `EXIT=0` (this is what ACP=932 is for).

## Assert red → dialog hang (#210)

A failing `assert` in an MSVC Debug build raises an abort dialog and hangs
forever. To capture a red run:

1. Redirect stderr to a log **on the Windows side** (inside the bat).
2. Run under WSL `timeout`, e.g. `timeout 60 cmd.exe /c run-tests.bat`.
3. Reap with `taskkill.exe /IM LokaTestsWin32.exe /F`.
4. Read the assert message (`Assertion failed: ... file:line`) from the log.

This is also the mutation-testing loop: mutate → red assert captured from the
log → restore → green (precedent: #211, `plans/199-mutation-log.md` in ~/loka).

## Screenshots (visual verification)

Capture with a PowerShell script (`Add-Type` + `System.Drawing`/GDI screen
copy, or the repo's `scripts/mame-run.ps1` style as a reference), inspect the
PNG by Reading it. For PR embedding, push assets to the `pr-assets` branch and
link raw URLs (example: the comment thread on #162).
