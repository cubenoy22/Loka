# DTP Sketch — PSDocument as a Loka Container

Exploration of reusing the Loka UI framework as a foundation for DTP (Desktop Publishing).

## API Sketch

```cpp
PSDocument() // Special canvas container for DTP
  << Box()
       .attr(BoxAttr()
               .size(Length::pt(100), Length::pt(100))
               .position(Length::pt(50), Length::pt(50)) // Absolute coordinates within canvas
               .angle(45)                                // Rotation angle
               .stroke(Length::pt(1), Color::Black)
               .fill(Color::White))
  << Text("Loka DTP")
       .attr(TextAttr()
               .fontSize(Length::pt(24))
               .position(Length::pt(100), Length::pt(200))
               .angle(-15));                             // Text rotation
```

## Alignment with Existing Design

### PSDocument as an UNCONSTRAINED Container

- Same pattern as `ScrollView` making one axis UNCONSTRAINED
- `PSDocument` can be defined as an absolute-positioning container with both axes UNCONSTRAINED
- Fits on top of the existing Layout Definition framework without introducing new concepts

### Ownership of position()

- Current draft rule: parent layout determines child position
- DTP absolute coordinates require children to declare their own position
- Define `PSDocument` as a special container that respects child attr `position` — this keeps it within the existing rules as "parent-permitted" rather than an exception

### angle() as Extended/Pro Attr

- Rotation is unnecessary for typical UI elements but essential for DTP
- Follows the `layout_attr_draft.md` policy: "heavy expressions are separated into Extended*Attr"
- Keeps standard `CommonAttr` clean; only DTP elements carry the rotation attribute

### Layer Ordering

- `ZStack` + `zIndex` mechanism applies directly
- DTP use cases involve frequent reordering via a layer panel, so dynamic `zIndex` changes are expected

### Length with PT/MM

- `Length::pt(72)` can be shared between screen preview and PostScript output
- The most practical benefit for DTP usage

### 68k/Classic Considerations

- QuickDraw does not natively support rotated rectangle drawing (requires CopyBits workarounds)
- Backend absorbs this complexity
- Toolbox backend can warn on `angle != 0`, or emulate rotation via CopyBits — contained within platform implementation

## Significance

Validates that the UI framework foundation can be repurposed as a graphic editor foundation. Serves as a proof of architectural generality.

## Status

Conceptual. Low implementation priority.
