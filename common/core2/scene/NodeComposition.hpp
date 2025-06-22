#ifndef DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP

#include <vector>

namespace declara
{
  namespace core
  {
    namespace scene
    {

      // 遅延評価イテレータアダプタ例
      // （本体は別ファイルで実装推奨。ここでは型宣言のみ）
      template <class It>
      class StreamView;

      // group関数: 複数ノードをまとめて返す（NodeList型を想定）
      template <typename NodeListT>
      NodeListT &group(NodeListT &nodes)
      {
        // パススルー。意図明示のための補助関数
        return nodes;
      }

      struct NodeComposition
      {
        // UIツリーのルートノードを保持
        void *root; // 仮: 実際はSceneNodeGroup*やNode*等に差し替え予定

        template <typename T>
        T &declare(T &x)
        {
          root = &x;
          return x;
        }

        // vector等から遅延評価ストリームを生成
        template <typename T>
        StreamView<typename std::vector<T>::const_iterator> stream(const std::vector<T> &v)
        {
          return StreamView<typename std::vector<T>::const_iterator>(v.begin(), v.end());
        }
        // begin/endイテレータからも生成可能
        template <typename It>
        StreamView<It> stream(It begin, It end)
        {
          return StreamView<It>(begin, end);
        }

        // group関数をメンバとしても提供
        template <typename NodeListT>
        NodeListT &group(NodeListT &nodes) { return group<NodeListT>(nodes); }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
