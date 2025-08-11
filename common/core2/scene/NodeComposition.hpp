#ifndef DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP

#include <vector>
#include "core2/scene/node/Conditional.hpp"

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

        template <typename T>
        declara::core::scene::ConditionalDefinition conditional(const State<bool> &condition, T &x)
        {
          extern T Empty;
          return ConditionalDefinition(ConditionalProps(&condition, &x, &Empty));
        }

        template <typename T>
        declara::core::scene::ConditionalDefinition conditional(State<bool> &condition, T &x)
        {
          extern T Empty;
          return ConditionalDefinition(ConditionalProps(&condition, &x, &Empty));
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

        template <typename T>
        T &group(T &x) { return x; }

        template <typename Stream, typename Func>
        auto map(const Stream &stream, Func func) -> decltype(stream.map(func))
        {
          return stream.map(func);
        }

        template <typename Stream, typename Pred>
        auto filter(const Stream &stream, Pred pred) -> decltype(stream.filter(pred))
        {
          return stream.filter(pred);
        }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
