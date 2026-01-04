# FIXME

- `ComposableNode::AttachedContext` から `BoundaryNode` などのメンバーアクセスで clangd が incomplete type 警告を出す。ビルドは通るが、必要なら `ComposableNode.hpp` での include 追加や型分離で抑制する。
