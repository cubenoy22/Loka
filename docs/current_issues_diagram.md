# Current Issues 図解

このファイルは [`CURRENT_ISSUES.md`](../CURRENT_ISSUES.md) の補助資料です。
主に「責務の置き場所」「データ寿命」「1 パス限りの解釈」を図で整理します。

## 1. 永続情報と一時情報の分離

```mermaid
flowchart TD
    A[retained な Node / NodeContext] --> B[永続的な lifecycle 情報]
    B --> B1[attached / detached]
    B --> B2[presentation phase]
    B --> B3[retained native ownership]

    C[Boundary ローカルの compose/apply パス] --> D[一時的な解釈結果]
    D --> D1[attach への昇格]
    D --> D2[child disposition]
    D --> D3[local rebuild / apply plan]
    D --> D4[temporary diff result]

    E[子 Node] --> F[1 パス限りの dispatch truth はここに置かない]
    F --> F1[composeAttachState_ を避ける]
    F --> F2[consume ベースの lifecycle hint を避ける]
```

## 2. 現状の臭いと目標形

```mermaid
flowchart LR
    subgraph 現状
        C1[Conditional / Show] --> C2[child node に mark]
        C2 --> C3[Node 上の composeAttachState_]
        C3 --> C4[Boundary が consume して clear]
        C4 --> C5[Platform 側コメントで補完]
    end

    subgraph 目標
        T1[Conditional / Show] --> T2[Boundary ローカルへ登録]
        T2 --> T3[Compose / Apply 用の working scope]
        T3 --> T4[Boundary が child disposition を導出]
        T4 --> T5[Traversal が disposition を参照]
        T5 --> T6[パス終了時に一時情報を破棄]
    end
```

## 3. Dynamic から Static へのヒント

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

    subgraph Static の Show / Conditional
        S1[compose 後の current structure]
        S2[縮小版 apply phase]
        S3[child compose disposition]
        S1 --> S2
        S2 --> S3
    end

    H[共通原則: パス限りの truth は Boundary が持つ]
    D4 --> H
    S3 --> H
```

## 4. 足りないピースが挟まる場所

```mermaid
sequenceDiagram
    participant ST as StateTracker
    participant B as Boundary
    participant C as Compose パス
    participant A as 縮小版 apply phase
    participant SC as Scene
    participant PC as PlatformController

    ST->>B: dirty source を検知
    B->>C: current structure を compose
    C->>A: Boundary ローカルの一時情報を渡す
    A->>A: child disposition を導出
    A->>SC: BoundaryProjection を渡す
    SC->>PC: SceneProjection を渡す

    Note over A: Static の Show / Conditional に足りない<br/>ローカル phase はここ
```

## 5. `ForEach<T>` への将来拡張

```mermaid
flowchart TD
    A[Show] --> A1[0/1 child disposition]
    B[Conditional] --> B1[branch disposition]
    C[ForEach<T>] --> C1[homogeneous な repeated-child disposition]

    A1 --> D[Boundary ローカルの縮小版 apply モデル]
    B1 --> D
    C1 --> D

    D --> E[v1 の意味論]
    E --> E1[attach]
    E --> E2[retain]
    E --> E3[retire]

    D --> F[必要なら後で拡張]
    F --> F1[replace]
    F --> F2[move / reorder]
    F --> F3[より高度な reuse policy]
```

## 6. 命名の方向

```mermaid
flowchart LR
    A[previous/current の比較] --> B[compare... / build...Diff]
    C[そのパス限りの解釈] --> D[derive...Disposition / build...Projection]

    E[operator- 的な曖昧さは避ける] --> B
    E --> D
```

## 7. `markDirty` から反映までの流れ

```mermaid
sequenceDiagram
    participant S as MutableState::set()
    participant T as StateTracker
    participant B as Boundary
    participant C as Compose
    participant A as 縮小版 apply phase
    participant SC as Scene
    participant PC as PlatformController

    S->>T: notify + markDirty(this)
    T->>B: dirty source / dirty flags を伝える
    B->>C: compose current structure
    C->>A: 1 パス限りの一時情報を渡す
    A->>A: child disposition / attach 昇格を導出
    A->>SC: compose/apply 結果を要約
    SC->>PC: platform に必要な結果だけ渡す

    Note over T,A: 問題の中心は markDirty 自体ではなく、<br/>dirty 検知の後の局所解釈をどこが持つか
```

## 8. `markDirty` の現状と理想

```mermaid
flowchart LR
    subgraph 現状
        C1[MutableState set] --> C2[StateTracker markDirty]
        C2 --> C3[Boundary / Scene が進む]
        C3 --> C4[Show / Conditional が child node に一時情報を置く]
        C4 --> C5[Boundary が consume]
    end

    subgraph 理想
        T1[MutableState set] --> T2[StateTracker markDirty]
        T2 --> T3[Boundary が dirty を受ける]
        T3 --> T4[Boundary ローカル phase で解釈]
        T4 --> T5[child disposition / projection を導出]
        T5 --> T6[consume 不要]
    end
```

## 9. Projection の三層

```mermaid
flowchart TD
    A[Boundary] --> B[BoundaryProjection]
    B --> C[Scene]
    C --> D[SceneProjection]
    D --> E[PlatformController]
    E --> F[platform execution payload / command stream]
    F --> G[Native UI / OS / Browser / Remote]
```

## 10. Update 全体像

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

## 11. Scene が大事になる理由

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

## 12. 将来のスレッド分離

```mermaid
flowchart LR
    A[Main Thread<br/>StateTracker / Boundary / Scene] --> B[SceneProjection]
    B --> C[Main Thread<br/>Platform command generation]
    C --> D[Optional Serial Worker<br/>Command optimization]
    D --> E[Main Thread / Platform Thread<br/>Apply]
```

## 13. 論理UIと遅延ホストの関係

```mermaid
flowchart LR
    A[論理UI<br/>SSoT] --> B[Projection / ObservationTransaction]
    B --> C[Observed Host<br/>Native UI / OS / Browser / Remote]

    C --> D[callback / result]
    D --> E{request / generation は<br/>まだ有効か}
    E -->|yes| F[論理UIへ反映]
    E -->|no| G[stale result を破棄]
```

## 14. `nextTick` と将来の transaction の役割分担

```mermaid
flowchart TD
    A[State change] --> B[nextTick]
    B --> C[flush のタイミングをまとめる]

    A --> D[Projection / ObservationTransaction]
    D --> E[今回の更新内容を保持]
    E --> E1[dirty targets]
    E --> E2[update roots]
    E --> E3[layout / paint / structure facts]
    E --> E4[request / generation facts]

    C --> F[flush]
    E --> F
    F --> G[Host へ apply]
```

## 15. 寿命と owner の原則

```mermaid
flowchart TD
    A[1 event cycle だけ有効] --> A1[stack local helper]
    A --> A2[Boundary local temporary scope]
    A --> A3[one-shot analysis / snapshot]

    B[event cycle を跨ぐ] --> B1[owner 必須]
    B1 --> B2[Scene]
    B1 --> B3[SceneDirector]
    B1 --> B4[Boundary]
    B1 --> B5[retained platform context]

    C[owner のない deferred / heap object] --> D[疑うべき設計]
```

## 16. Transaction の起点と commit

```mermaid
flowchart LR
    A[State / Event] --> B[Boundary 近傍で dirty 検知]
    B --> C[Boundary が enqueue / emit]
    C --> D[Scene-owned pending transaction]
    D --> E[Scene flush / commit]
    E --> F[Platform apply]

    G[Scene switch / Window dispose / unmount] --> H[Scene が pending を cancel]
    H --> D
```

## 15. 火星通信メタファ

```mermaid
sequenceDiagram
    participant L as 論理UI
    participant P as Projection
    participant H as Host / OS

    L->>P: 今の truth から像を作る
    P->>H: 遅れて届く
    H-->>P: 古い result が返ることがある
    P->>L: generation 一致なら反映
    P->>L: 不一致なら破棄

    Note over L,H: host が見ているのは常に少し遅れた像<br/>truth は論理UIにのみある
```
