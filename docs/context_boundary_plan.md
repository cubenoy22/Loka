# Context & Boundary Plan (WIP)

## 現状整理
- NodeComposition は compose 中に `NodeDefinition` を clone する“スナップショット”。実体の Node/Context は ComposableNode (Boundary) が保持する。
- Context API: `ComposableNode::useContext(def, placement)` で Context を自生。`ContextPlacement` (BOUNDARY/ROOT) で所属を切替。

## 目標
1. **Boundary 正式化**
   - ComposableNode が自前の `NodeComposition`/Context を持ち、Boundary ごとに compose→materialize を完結。

2. **Context DSL の簡素化**
   - `this->useContext` + `c.useContext` をヘルパひとつ (`exposeContext`) で済ませ、DSL からは 1 行で Context を提供できるようにする。
   - Context を NodeDefinition/Headless と同じ仕組みに寄せ、`ContextDefinition -> ContextNode` のペアで扱う。

3. **所有権の明確化**
   - Scene が root Boundary を所有し、Window は Scene の差替えのみ意識。
   - Context/Node は各 Boundary (ComposableNode) が完全に所有し、NodeComposition は compose 中のみ生きる一時オブジェクトにとどめる。

## TODO
- [ ] `exposeContext` のようなラッパを追加して DSL の呼び味を統一する。
- [x] Boundary (ComposableNode) に専用の `NodeComposition` メンバーを追加し、Boundary ごとに compose を完結する。（`ComposableNode::beginComposition` で境界ごとのアリーナを使い回せるようになった。）
- [x] BoundaryNode を導入し、境界ごとに `StateTracker` を所有できるようにする（StaticCompositionBoundary を派生させる）。
- [ ] Window close request を Scene/Root controller に委譲し、未保存時のキャンセル判断をできるようにする。
- [ ] MutableState の生成を Boundary 経由に限定し、Tracker 登録漏れを文法レベルで防ぐ（StateAllocatable + friend の設計検討）。
- [ ] ContextDefinition/ComponentContext の公開 API を廃止し、findBoundary に集約する。
- [ ] Context を Headless Node として表現する `ContextDefinition/ContextNode` の設計を検討する。
