# Update Pipeline Overview

This note organizes the main ideas around Loka's update / compose / apply flow
as diagrams.

It mainly covers:

- where responsibilities live
- how retained information differs from one-pass temporary interpretation
- what Boundary / Scene / Platform hand to each other

## 1. Persistent vs Temporary Information

```mermaid
flowchart TD
    A[retained Node / NodeContext] --> B[persistent lifecycle information]
    B --> B1[attached / detached]
    B --> B2[presentation phase]
    B --> B3[retained native ownership]

    C[boundary-local compose/apply pass] --> D[temporary interpretation result]
    D --> D1[promote to attach]
    D --> D2[child disposition]
    D --> D3[local rebuild / apply plan]
    D --> D4[temporary diff result]

    E[child node] --> F[do not store one-pass dispatch truth in naked fields]
    F --> F1[keep it inside a small state machine such as ComposeAttachLifecycle]
    F --> F2[callers should use only resolve-style APIs]
```

## 2. Current Shape and Intended Target

```mermaid
flowchart LR
    subgraph Current
        C1[Conditional / Show] --> C2[mark the child node]
        C2 --> C3[ComposeAttachLifecycle on Node]
        C3 --> C4[Boundary resolves the child compose event]
        C4 --> C5[do not expose lifecycle hints to Platform]
    end

    subgraph Target
        T1[Conditional / Show] --> T2[explicit lifecycle state machine]
        T2 --> T3[working scope for Compose / Apply]
        T3 --> T4[Boundary derives the child compose event]
        T4 --> T5[Traversal uses the resolved event]
        T5 --> T6[temporary information is discarded inside the lifecycle / scope]
    end
```

## 3. Hints from Dynamic to Static

```mermaid
flowchart TD
    subgraph DynamicComposition
        D1[previous snapshot]
        D2[current snapshot]
        D3[NodeCompositionDiff]
        D4[LocalRebuildPlan]
        D1 --> D3
        D2 --> D3
        D3 --> D4
    end

    subgraph Static Show / Conditional
        S1[current structure after compose]
        S2[reduced apply phase]
        S3[child compose disposition]
        S1 --> S2
        S2 --> S3
    end

    H[shared rule: one-pass truth belongs to Boundary]
    D4 --> H
    S3 --> H
```

## 4. Where the Local Phase Fits

```mermaid
sequenceDiagram
    participant ST as StateTracker
    participant B as Boundary
    participant C as Compose pass
    participant A as reduced apply phase
    participant SC as Scene
    participant PC as PlatformController

    ST->>B: detect dirty source
    B->>C: compose current structure
    C->>A: pass boundary-local temporary information
    A->>A: derive child disposition
    A->>SC: pass BoundaryProjection
    SC->>PC: pass SceneProjection

    Note over A: Even for static Show / Conditional,<br/>the local phase belongs here
```

## 5. Future Extension Toward `ForEach<T>`

```mermaid
flowchart TD
    A[Show] --> A1[0/1 child disposition]
    B[Conditional] --> B1[branch disposition]
    C[ForEach<T>] --> C1[homogeneous repeated-child disposition]

    A1 --> D[boundary-local reduced apply model]
    B1 --> D
    C1 --> D

    D --> E[v1 semantics]
    E --> E1[attach]
    E --> E2[retain]
    E --> E3[retire]

    D --> F[extend later if needed]
    F --> F1[replace]
    F --> F2[move / reorder]
    F --> F3[more advanced reuse policy]
```

## 6. Naming Direction

```mermaid
flowchart LR
    A[compare previous/current] --> B[compare... / build...Diff]
    C[one-pass interpretation] --> D[derive...Disposition / build...Projection]

    E[avoid operator-like ambiguity] --> B
    E --> D
```

## 7. Flow from `markDirty` to Reflection

```mermaid
sequenceDiagram
    participant S as MutableState::set()
    participant T as StateTracker
    participant B as Boundary
    participant C as Compose
    participant A as reduced apply phase
    participant SC as Scene
    participant PC as PlatformController

    S->>T: notify + markDirty(this)
    T->>B: pass dirty source / dirty flags
    B->>C: compose current structure
    C->>A: pass one-pass temporary information
    A->>A: derive child disposition / attach promotion
    A->>SC: summarize compose/apply results
    SC->>PC: pass only what the platform needs

    Note over T,A: More important than markDirty itself<br/>is where the local interpretation after dirty detection lives
```

## 8. A Cleaner View of `markDirty`

```mermaid
flowchart LR
    subgraph Current
        C1[MutableState set] --> C2[StateTracker markDirty]
        C2 --> C3[Boundary / Scene proceeds]
        C3 --> C4[Show / Conditional stores temporary information on the child node]
        C4 --> C5[Boundary resolves the compose event]
    end

    subgraph Ideal
        T1[MutableState set] --> T2[StateTracker markDirty]
        T2 --> T3[Boundary receives the dirty signal]
        T3 --> T4[Boundary interprets it in a local phase]
        T4 --> T5[derive child disposition / projection]
        T5 --> T6[no manual consume step needed]
    end
```

## 9. Three Layers of Projection

```mermaid
flowchart TD
    A[Boundary] --> B[BoundaryProjection]
    B --> C[Scene]
    C --> D[SceneProjection]
    D --> E[PlatformController]
    E --> F[platform execution payload / command stream]
    F --> G[Native UI / OS / Browser / Remote]
```

## 10. Overall Update Flow

```mermaid
flowchart TD
    A[State change / Event] --> B[StateTracker markDirty]
    B --> C[Boundary compose]
    C --> D[Boundary local apply phase]
    D --> E[BoundaryProjection]

    E --> F[Scene enqueueBoundaryProjection]
    F --> G[Scene build / combine]
    G --> H[SceneProjection]

    H --> I[Platform command generation]
    I --> J[Optional command optimization]
    J --> K[PlatformController applySceneProjection]
    K --> L[Native UI / OS / Browser / Remote Target]
```

## 11. Why Scene Matters

```mermaid
flowchart LR
    A[Boundary local truth] --> B[BoundaryProjection]
    B --> C[Scene gathers cross-boundary effects]
    C --> D[SceneProjection]
    D --> E[Platform apply]

    F[nested boundary event]
    G[sibling relayout/repaint]
    H[scene switch / root replacement]

    F --> C
    G --> C
    H --> C
```

## 12. Future Thread Separation

```mermaid
flowchart LR
    A[Main Thread<br/>StateTracker / Boundary / Scene] --> B[SceneProjection]
    B --> C[Main Thread<br/>Platform command generation]
    C --> D[Optional Serial Worker<br/>Command optimization]
    D --> E[Main Thread / Platform Thread<br/>Apply]
```

## 13. Logical UI and the Delayed Host

```mermaid
flowchart LR
    A[logical UI<br/>SSoT] --> B[Projection / ObservationTransaction]
    B --> C[Observed Host<br/>Native UI / OS / Browser / Remote]

    C --> D[callback / result]
    D --> E{is the request / generation<br/>still valid?}
    E -->|yes| F[reflect into logical UI]
    E -->|no| G[discard stale result]
```

## 14. Division of Roles Between `nextTick` and Future Transactions

```mermaid
flowchart TD
    A[State change] --> B[nextTick]
    B --> C[collect flush timing]

    A --> D[Projection / ObservationTransaction]
    D --> E[hold the content of this update]
    E --> E1[dirty targets]
    E --> E2[update roots]
    E --> E3[layout / paint / structure facts]
    E --> E4[request / generation facts]

    C --> F[flush]
    E --> F
    F --> G[apply to host]
```

## 15. Lifetime and Owner Rules

```mermaid
flowchart TD
    A[valid for only one event cycle] --> A1[stack-local helper]
    A --> A2[boundary-local temporary scope]
    A --> A3[one-shot analysis / snapshot]

    B[crosses event cycles] --> B1[must have an owner]
    B1 --> B2[Scene]
    B1 --> B3[SceneDirector]
    B1 --> B4[Boundary]
    B1 --> B5[retained platform context]

    C[deferred / heap object without an owner] --> D[design should be questioned]
```

## 16. Transaction Start and Commit

```mermaid
flowchart LR
    A[State / Event] --> B[dirty detected near Boundary]
    B --> C[Boundary enqueues / emits]
    C --> D[pending transaction owned by Scene]
    D --> E[Scene flush / commit]
    E --> F[Platform apply]

    G[Scene switch / Window dispose / unmount] --> H[Scene cancels pending work]
    H --> D
```

## 17. Mars Communication Metaphor

```mermaid
sequenceDiagram
    participant L as logical UI
    participant P as Projection
    participant H as Host / OS

    L->>P: build an image from current truth
    P->>H: it arrives late
    H-->>P: sometimes an old result comes back
    P->>L: reflect it if the generation matches
    P->>L: discard it if the generation does not match

    Note over L,H: the host always sees a slightly delayed image<br/>truth lives only in the logical UI
```
