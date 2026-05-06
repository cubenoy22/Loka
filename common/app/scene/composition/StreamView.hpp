#ifndef LOKA_CORE2_SCENE_COMPOSITION_STREAMVIEW_HPP
#define LOKA_CORE2_SCENE_COMPOSITION_STREAMVIEW_HPP

#include <iterator>

namespace loka
{
  namespace app
  {
    namespace scene
    {

      // C++98対応: 遅延評価イテレータ風の薄いビュー

      template <class It>
      class StreamView
      {
      public:
        typedef It iterator;
        typedef typename std::iterator_traits<It>::value_type value_type;

        StreamView(It begin, It end) : begin_(begin), end_(end) {}

        // 付加操作（filter/map/each/toVector）は未提供（必要時に拡張）

        It begin() const { return begin_; }
        It end() const { return end_; }

      private:
        It begin_, end_;
      };

      // 追加アダプタは未実装

      // MapIterator は C++98 互換の型推論困難のため未提供

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_COMPOSITION_STREAMVIEW_HPP
