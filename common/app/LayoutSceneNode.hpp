#ifndef DECLARA_LAYOUTSCENENODE_HPP
#define DECLARA_LAYOUTSCENENODE_HPP

#include "core/util/SceneNodeAttachScope.hpp"
#include "core/SceneNodeFactory.hpp"
#include "app/ContainerSceneNode.hpp"
#include <vector>

class SceneNode;

struct LayoutSceneNodeTypeTag
{
};

// --- Box: 縦横アラインメント・サイズ指定可能なレイアウトエンジン ---
class Box /*/: public TreedSceneComponent*/
{
public:
  enum Direction
  {
    Vertical,
    Horizontal
  };
  enum Alignment
  {
    Start,
    Center,
    End,
    Stretch
  };

  Box(Direction dir = Vertical)
  {
    direction = dir;
    alignment = Start;
    width = -1;
    height = -1;
  }

  Box &setDirection(Direction dir)
  {
    direction = dir;
    return *this;
  }
  Box &setAlignment(Alignment align)
  {
    alignment = align;
    return *this;
  }
  Box &setWidth(int w)
  {
    width = w;
    return *this;
  }
  Box &setHeight(int h)
  {
    height = h;
    return *this;
  }

  // 差分検知・再描画
  // void update() override
  // {
  //   // レイアウト計算（仮実装）
  //   for (auto *c : children)
  //   {
  //     if (c->isDirty())
  //       c->update();
  //   }
  // }

  // ...描画・レイアウト計算用の追加メソッド...

protected:
  Direction direction;
  Alignment alignment;
  int width, height; // -1: 自動
};

// --- LayoutSceneNode: Boxレイアウトを担うSceneNode ---
class LayoutSceneNode : public ContainerSceneNode
{
public:
  typedef LayoutSceneNodeTypeTag TypeTag;

  LayoutSceneNode(Box::Direction dir = Box::Vertical);

  void setDirection(Box::Direction dir) { box_.setDirection(dir); }
  void setAlignment(Box::Alignment align) { box_.setAlignment(align); }
  void setWidth(int w) { box_.setWidth(w); }
  void setHeight(int h) { box_.setHeight(h); }

  // レイアウト計算・配置処理（仮実装）
  void layout()
  {
    // ここでbox_の設定に従いchildren_を配置する
    // 実際の描画や座標計算はbackendで拡張予定
  }

protected:
  Box box_;
};

struct LayoutSceneNodeProps : public NodePropsBase<LayoutSceneNodeProps>
{
  typedef LayoutSceneNodeTypeTag TypeTag;

  // 必要に応じてレイアウト方向やサイズ等のプロパティを追加可能
  // 例: int direction, int spacing, ...
  static SceneNode *createNode(const LayoutSceneNodeProps &props);

  int hash() const
  {
    // 現状は全インスタンス同一扱いなので定数値
    return 0;
  }

  bool operator<(const PropsBase &rhs) const
  {
    // 現状は全インスタンス同一扱い（拡張時は比較ロジック追加）
    return false;
  }
};

inline SceneNode *LayoutSceneNodeProps::createNode(const LayoutSceneNodeProps &props) { return new LayoutSceneNode(); }

#include "core/SceneNodeFactory.hpp"
struct LayoutSceneNodeProps;
typedef NodeDefinition<LayoutSceneNodeProps, LayoutSceneNode> LayoutSceneNodeDefinition;

#endif // DECLARA_LAYOUTSCENENODE_HPP
