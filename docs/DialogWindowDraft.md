# Dialog And Window Draft

This draft captures early design notes for dialogs, file choosers, alerts, and
future dynamic window declarations. It is not a stable API contract.

## Problem

Native dialogs are not ordinary scene children.

A file dialog, alert, save panel, color picker, or custom modal view may be
requested from a Scene, but its actual presentation is usually owned by an App
or Window-level platform surface. Many platforms naturally allow only one active
dialog per owner, or require the native parent window, focus state, activation
state, modal loop, or sheet relationship to be handled outside the logical Scene
tree.

If every Scene subtree can freely declare native dialog nodes, an application can
accidentally create many presenters for a capability that should normally be
unique per App or Window. That can lead to confusing modal state, unexpected
multiple native dialogs, larger binaries on retro targets, and ownership that is
not visible from the DSL.

`OpenFileDialog` is currently the main experiment in this area. `MsgBox` should
not grow as another ad hoc helper before the ownership and presentation contract
is clearer.

## Direction

Prefer App/Window-owned presentation declarations over Scene-owned native dialog
instances.

The App or Window should declare the dialog capabilities/presenters it owns. A
Scene or Boundary should request those presenters through State, EmitterState,
Flow, or an explicit request object. The native platform context belongs to the
App/Window presenter, while request and result state remain owned by the
appropriate application owner.

Conceptually:

```text
App / Window
  owns dialog presenters and platform presentation policy
  owns default/custom presenter availability

Scene / Boundary
  declares buttons and UI state
  emits dialog requests
  observes dialog results
```

This keeps presentation ownership visible and prevents a Scene from implicitly
creating native presenters just because several subtrees mention a file dialog.

## Link By Declaration

Avoid introducing a separate `Link` concept unless the DSL needs it. The simpler
principle is:

> If a dialog/presenter appears in the App or Window declaration, the
> application uses that capability. If it does not appear, the presenter and
> platform code should not be pulled in by ordinary Scene code.

This matters for small and retro targets. A Loka application should not grow
unused platform dialog code merely because a framework header is available.

Example shape, not final API:

```cpp
Window()
  << Dialogs()
       << FileOpenDialog()
            .request(this->openFileRequested_)
            .result(this->openFileResult_);
```

Scene code can then trigger the request:

```cpp
Button("Open")
  .onClick(this->openFileRequested_);
```

The important point is that the presenter is declared once at the App/Window
level. The Scene does not own a native dialog instance.

## Native And Custom Dialogs

Default native dialogs and custom declarative dialogs should follow the same
ownership shape where possible:

- App/Window owns the presenter or custom dialog region.
- Scene/Boundary emits requests and consumes results.
- A custom dialog can be shown with State-driven visibility inside an
  App/Window-owned dialog region.
- A native dialog can be presented by a platform-specific presenter registered
  in the same App/Window-owned structure.

This lets an application replace a default native file dialog with a custom
dialog without changing the ownership model.

## Concurrency Policy

The presenter owner should define what happens when multiple requests arrive:

- ignore while active;
- replace current request;
- queue requests;
- reject with a diagnostic/result;
- allow multiple presenters only when explicitly declared as separate
  App/Window-owned entries.

The default should be conservative. Most built-in dialogs should not allow
accidental multiple active native dialogs from several Scene subtrees.

## OpenFileDialog

`OpenFileDialog` is useful as a first experiment, but its long-term shape is
still open.

Likely direction:

- keep request/result state app-facing and explicit;
- move native presentation ownership toward App/Window presenter structures;
- keep platform callback delivery lifecycle-aware and guarded against stale
  Scene/Boundary state;
- avoid treating file dialogs as ordinary visual nodes in arbitrary Scene
  subtrees unless they are purely custom declarative dialogs.

## Alert And Message Dialogs

`MsgBox`-style helpers are tempting because they are small, but they hide too
much:

- who owns the modal presenter;
- whether multiple dialogs can be active;
- how result delivery is scheduled;
- how Classic Toolbox resource or pseudo-dialog strategies work;
- whether the API is native-only or replaceable with a custom declarative
  dialog.

Future alert/message dialog support should be designed as an App/Window-owned
presenter or custom dialog region, not as a standalone helper that directly
invokes platform APIs from arbitrary code.

Classic Toolbox support may require resource-based dialogs, Alert Manager, or a
Loka-rendered pseudo-dialog fallback. That choice should remain behind the
presenter/projection seam.

## Open Questions

- Should dialog presenters be owned by App, Window, or both?
- What is the first stable shape for App/Window-owned presenter declarations?
- Should Scene-side request nodes exist, or should buttons/events update
  App/Window-owned request State directly?
- How should active native dialogs be canceled when their requesting Scene or
  Boundary detaches?
- How should presenter declarations interact with future dynamic Window
  declarations?
- Can build/link configuration reliably avoid unused platform presenter code on
  retro targets without adding a separate Link DSL?
