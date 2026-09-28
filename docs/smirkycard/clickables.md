# Seats and clickable controls

Status: Guide. Code truth: `example/SmirkyCard/src/CardRecords.hpp`,
`JsCardNode.cpp`, and `JsClickNode.hpp`. Verification:
`example/SmirkyCard/tests/SmirkyCardTests.cpp`.

The [SmirkyCard JS API](../../example/SmirkyCard/README.md#js-seats-and-clickables)
defines the shared clickable budget and seat declaration rules.

```js
card('first', class {
  constructor() {
    this.caption = state('.');
  }
  compose() {
    return Cell(this.caption, () => this.caption.set('X'));
  }
});
```

`Cell(textOrSeat, handler)` renders the same native text Cell as C++ MineSweeper.
Its label can be a literal String or a live String seat. It does not expose
Button's enabled modifier.

## Grids

`Grid(rows, cols, children)` uses the existing C++ `loka::app::Grid` layout.
Dimensions are numeric integers in 1..16; the one-level-flattened child count
must equal their product, up to 256. Nested arrays, invalid dimensions and
count mismatches refuse the card with a descriptive message. The separate
seat, clickable and depth budgets still apply; stacks keep their 16-child cap.

The real [MINES.JS](../../example/SmirkyCard/MINES.JS) composes a status Text,
a Row of three Buttons, and `Grid(8, 8, cells)` last in its VStack, mirroring
the C++ MineSweeper's Column with controls before its board. Column offers
each child the remaining height; Row advances to the greatest child bottom.
On macOS Buttons return their fixed control height plus spacing, so the
button row stays compact. The final Grid divides the remaining space into
equal cells. A Row of Cells instead consumes the offered height.

Code truth: `JsCardBindingRegistry.cpp`, `common/app/layout/ColumnLayout.hpp`,
`RowLayout.hpp`, `GridLayout.hpp`, and
`apple/macos/src/context/MacButtonContext.mm`. Host tests pin Grid dimensions,
child order, refusal messages and the real game's structure; native appearance
is checked separately on the macOS rig.
