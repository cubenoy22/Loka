# PR #670 Verification Captures

## macOS Tahoe 26.6.2 — LokaSmirkyCardMacOS (21b8d352)

Baseline and AX click sequence captures (20 alternating card clicks):

- **tahoe-baseline.png** — Initial state, card 1 visible
- **tahoe-click5.png** — After 5 clicks
- **tahoe-click10.png** — After 10 clicks
- **tahoe-click15.png** — After 15 clicks
- **tahoe-click20.png** — After 20 clicks
- **tahoe-frontmost-diag.png** — Frontmost AX diagnostic after test completion

All macOS captures show rect 60,90,420×272 unchanged and clean exit with no crash report.

## Win32 Omen — LokaSmirkyCardWin32 (d72540c6)

Click sequence captures (button HWND recreated on each card switch):

- **win32-smirkycard-click5.png** — After 5 clicks
- **win32-smirkycard-click10.png** — After 10 clicks
- **win32-smirkycard-click15.png** — After 15 clicks
- **win32-smirkycard-click20.png** — After 20 clicks
- **win32-run-log.txt** — Test execution log

All Win32 SmirkyCard captures show rect unchanged and clean exit (exit 0).

## Win32 Omen — LokaSimpleViewerWin32 (d72540c6)

Four Open… cycles with hand-chosen files (responsive Fit/Actual Size):

- **win32-simpleviewer-00-initial.png** — Initial state
- **win32-simpleviewer-step1-after-manual.png** — After first file selection
- **win32-simpleviewer-step1-after-manual.png** — File chooser shows native dialog
- **win32-simpleviewer-step2-after.png** — After second Open cycle
- **win32-simpleviewer-step2-dialog-open.png** — File chooser (step 2)
- **win32-simpleviewer-step3-after.png** — After third Open cycle
- **win32-simpleviewer-step3-dialog-open.png** — File chooser (step 3)
- **win32-simpleviewer-step4-after.png** — After fourth Open cycle
- **win32-simpleviewer-step4-dialog-open.png** — File chooser (step 4)
- **win32-simpleviewer-05-fit.png** — Fit to window mode
- **win32-simpleviewer-06-actualsize.png** — Actual size mode

SimpleViewer cycles show clean behavior, no assert, and responsive Fit/Actual Size controls; Cancel button not exercised.

## Supporting Logs

- **win32-run-log.txt** — SmirkyCard and SimpleViewer test execution logs
