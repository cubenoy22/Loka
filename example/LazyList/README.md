# LazyList

A paged card list on `LazyColumn`, backed by an app-owned `ObservableList`.
Prev and Next move one page; Rename #3 edits the card currently at index 3.
Remove first, Insert top, and Move first to end replace the list generation.
Insertion into the initially full list reports a refusal; remove a card first.
The star button toggles local selection. A content rename preserves selection,
while paging away or changing structure destroys the materialization and its
selection. All strings and symbols are original; there are no third-party assets.

The app config and model share boot geometry, following SmirkBench's constructor
bounds pattern: a 640×244 content window, 8-pixel margins, a 40-pixel button bar,
a 624×160 list seat, and a 20-pixel status line with two 4-pixel gaps. The initial
content-coordinate viewport is `(0, 0, 624, 160)`, so eight 20-pixel cards appear
on the first mount. Cell dimensions and the list seat stay fixed when the window
resizes; this example demonstrates paging, not responsive cell sizing.

Build the default M = 100 example (unmount its disk image before rebuilding):

```sh
cmake --preset retro68-68k-release -DLAZYLIST_CAPACITY=100 -DLOKA_LAZYFLEX_MAX_ITEMS=256
cmake --build --preset retro68-68k-release --target LokaLazyList68K_APPL
```

For the M = 1000 measurement, both CMake cache values become compile definitions
for this example. Use a separate build tree to preserve the default size bank:

```sh
cmake --preset retro68-68k-release -B build/retro68/68k/LazyList1000 -DLAZYLIST_CAPACITY=1000 -DLOKA_LAZYFLEX_MAX_ITEMS=1000
cmake --build build/retro68/68k/LazyList1000 --target LokaLazyList68K_APPL
```

`LAZYLIST_CAPACITY` defaults to 100 and accepts 1..1000. Raising it to 300 without
raising `LOKA_LAZYFLEX_MAX_ITEMS` (default 256) produces a capacity-refused view
with no card controls. `LazyFlexNode::status()` reports `LAZY_FLEX_CAPACITY_REFUSED`.
The model edit doors return `ListEditResult`; paging returns `EDIT_OK` for a
clamped no-op and `EDIT_NOT_ATTACHED` if initialization refused. The status line
reports edit refusals. Rename counts travel in the card value; count or display
number exhaustion reports `EDIT_SEQ_EXHAUSTED` without changing the list.

The other application targets are `LokaLazyListMacOS` (`macos-debug`) and
`LokaLazyListWin32` (`win32-debug`). VS Code includes their build/run entries and
the Classic build/mount tasks. The 1 MiB Classic SIZE partition is a starting
budget requiring runtime measurement, especially at M = 1000.

Null pins live in `tests/LazyListTests.cpp` and `tests/LazyListCapacityTests.cpp`.
There is no MAME golden scenario cell and no EditText variant in this example.

The lifecycle review has four risk flags: a new model owner, several node-owned
State handles, State-to-Boundary routing, and live model borrows across the
Boundary. Existing framework scopes own all component teardown and native
retirement. The model must outlive the scene; paging and structural edits
invalidate each departing card's local state. Null tests pin these transitions.
Native presentation and Classic memory headroom remain provisional until the
Toolbox/MAME, macOS and Win32 runtime checks are completed.
