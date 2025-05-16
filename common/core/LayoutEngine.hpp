#ifndef DECLARA_LAYOUTENGINE_HPP
#define DECLARA_LAYOUTENGINE_HPP

#include <vector>
#include <string>

// --- Box: 縦横アラインメント・サイズ指定可能なレイアウトエンジン ---
class Box /*/: public TreedSceneComponent*/
{
public:
  enum class Direction
  {
    Vertical,
    Horizontal
  };
  enum class Alignment
  {
    Start,
    Center,
    End,
    Stretch
  };

  Box(Direction dir = Direction::Vertical)
      : direction(dir), alignment(Alignment::Start), width(-1), height(-1) {}

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

#endif // DECLARA_LAYOUTENGINE_HPP
