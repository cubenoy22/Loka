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
