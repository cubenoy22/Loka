# SceneManager Design Notes (2025-11)

This archived note captures the design context around `common/app/SceneManager.*`
at the time the scene-transition refactor was active. The code is the source of
truth; this file exists to preserve rationale and tradeoffs.

---

## 1. Structure At The Time

| Module | Role | Main members |
| --- | --- | --- |
| `SceneManager` | One instance per `Window`. Owns scene replacement and the transition queue. | `MutableState<Scene*> currentScene_`, `MutableState<std::vector<std::pair<Scene*, Scene*>>> pendingTransactions_`, `PushStateTracker tracker_` |
| `Window` | Owns `SceneManager` and exposes `sceneManager()->commitTransaction(...)` as the app-facing API. | `SceneManager sceneManager_` |
| `loka::app::scene::Scene` | Owns `SceneLifecycle` as `MutableState<SceneLifecycle>`, keeps a `BoundaryNode` root, and auto-wraps non-boundary roots. | `MutableState<SceneLifecycle> lifecycle_`, `BoundaryNode *rootNode_` |

At this point, `SceneManager` had already replaced the older
`SceneTransaction` / `SceneManagerDelegate` arrangement with a simpler
`from/to` queue plus tracker-driven notification.

---

## 2. Transition Flow

1. `commitTransaction(from, to)`

   - Push `(from, to)` into `pendingTransactions_`.
   - Wrap the mutation in `PushStateTracker::begin/end`, so observers of
     `pendingTransactions_` are notified automatically.
   - Call `handleNextTransaction()` immediately after enqueueing.

2. `handleNextTransaction()`

   - Read the head entry from `pendingTransactions_`; return if the queue is empty.
   - Call `swapScene(currentScene_.get(), tx.to)`.
   - Pop one entry from `pendingTransactions_`.
   - The implementation consumes one entry at a time. Multiple transitions are
     expected to arrive via repeated `commitTransaction(...)` calls.

3. `swapScene(old, next)`

   - Wrap `currentScene_.set(next)` in a tracker transaction.
   - Update `Scene::updateAttached(...)` and `Scene::updateLifecycle(...)`.
   - Express lifecycle transitions through observable state rather than direct
     attach/detach callbacks.

---

## 3. Window / Scene Integration

- `Window` constructs `SceneManager` and exposes
  `sceneManager_.getCurrentScene()` as `State<Scene*>`.
- `Window::scene()` is a thin wrapper over
  `sceneManager_.getCurrentScene().get()`.
- Under the Solid-mode direction described in `docs/design_minutes.md`,
  `Window::mount(CompositeNode&)` was intended to stay narrow in scope, so
  `SceneManager` remained focused on the window-level scene slot.

---

## 4. Known Issues At The Time

1. Lifecycle notification cleanup

   - `SceneManager::swapScene` already owned `Scene::updateAttached(...)` and
     `Scene::updateLifecycle(...)`.

2. Missing discard / requestDiscard flow

   - The older idea of delaying a transition until a save dialog completed did
     not exist in the current implementation.
   - A future direction was to extend the queued item to something like
     `(Scene*, PendingAction)` and let `Scene` signal completion through
     `EmitterState`.

3. Multiple queued transitions

   - Only one transition was popped per `handleNextTransaction()` call.
   - A burst of `commitTransaction(...)` calls therefore incurred repeated
     immediate processing overhead.
   - The noted options were:
     - process the queue in a `while (!pending.empty())` loop, or
     - defer to the next tick through tracker/scheduler integration.

---

## 5. Follow-up

Active TODO items were moved to `docs/TODO.md`.

---

## 6. References

- `common/app/SceneManager.hpp`
- `common/app/SceneManager.cpp`
- `common/app/Window.hpp`
- `common/app/scene/Scene.hpp`
- `docs/design_minutes.md`
