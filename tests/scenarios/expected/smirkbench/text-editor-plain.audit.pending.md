# Staged Toolbox editor cell (#853 PR 2)

No runtime or golden verdict is claimed. The maintainer runs:

```
tests/toolbox/run-scenario.sh smirkbench text-editor-plain --update-golden
```

This opens the SmirkBench **Plain text editor** card. The scripted sequence
clicks the start of line 2, types `x`, Return, `y`, Left, Backspace, and attempts
an 8193-byte paste. The terminal record must report `line2=xysecond`,
`line_counts=3->4->3`, `cap=refused`, `restores=1`. The native view must agree and use plain Monaco 9. The click uses the TE
record's line height, so it follows the selected font metrics.
The extra Left makes Backspace occur at the new line's start after typing `y`.

`run-scenario.sh` generates `LokaTest.cfg` with `scenario text-editor-plain`.
The entire golden bundle is re-baked and approved atomically; adding this
registry entry is staging only. macOS and Win32 remain unavailable until PR 3.
The PowerBook 180c leg is a maintainer observation of the same sequence.
