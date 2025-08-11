#ifndef DECLARA_CORE2_SCENE_STREAMVIEW_HPP
#define DECLARA_CORE2_SCENE_STREAMVIEW_HPP

#include <iterator>
#include <vector>

namespace declara
{
  namespace core
  {
    namespace scene
    {

      // --- FilterIterator/MapIteratorの前方宣言 ---
      template <class It, class FilterFunc>
      class FilterIterator;
      template <class It, class MapFunc>
      class MapIterator;

      // C++98対応: 遅延評価イテレータアダプタ
      // - filter/map/eachチェーンをサポート
      // - 中間配列を作らず、走査時に評価

      template <class It>
      class StreamView
      {
      public:
        typedef It iterator;
        typedef typename It::value_type value_type;

        StreamView(It begin, It end) : begin_(begin), end_(end) {}

        // filter: 条件を満たす要素のみを通す
        template <class FilterFunc>
        StreamView<FilterIterator<It, FilterFunc>> filter(const FilterFunc &f) const
        {
          return StreamView<FilterIterator<It, FilterFunc>>(
              FilterIterator<It, FilterFunc>(begin_, end_, f),
              FilterIterator<It, FilterFunc>(end_, end_, f));
        }

        // map: 要素を変換しながら返す
        template <class MapFunc>
        StreamView<MapIterator<It, MapFunc>> map(const MapFunc &f) const
        {
          return StreamView<MapIterator<It, MapFunc>>(
              MapIterator<It, MapFunc>(begin_, f),
              MapIterator<It, MapFunc>(end_, f));
        }

        // each: 各要素に副作用を与える（forEach的用途）
        template <class EachFunc>
        void each(const EachFunc &f) const
        {
          for (It it = begin_; it != end_; ++it)
            f(*it);
        }

        // toVector: 結果をvectorに格納
        std::vector<value_type> toVector() const
        {
          std::vector<value_type> result;
          for (It it = begin_; it != end_; ++it)
            result.push_back(*it);
          return result;
        }

        It begin() const { return begin_; }
        It end() const { return end_; }

      private:
        It begin_, end_;
      };

      // --- FilterIterator/MapIteratorの雛形（C++98対応） ---

      template <class It, class FilterFunc>
      class FilterIterator
      {
      public:
        typedef It iterator;
        typedef typename It::value_type value_type;
        FilterIterator(It cur, It end, const FilterFunc &f)
            : cur_(cur), end_(end), func_(f) { advance(); }
        value_type operator*() const { return *cur_; }
        FilterIterator &operator++()
        {
          ++cur_;
          advance();
          return *this;
        }
        bool operator!=(const FilterIterator &rhs) const { return cur_ != rhs.cur_; }

      private:
        void advance()
        {
          while (cur_ != end_ && !func_(*cur_))
            ++cur_;
        }
        It cur_, end_;
        FilterFunc func_;
      };

      template <class It, class MapFunc>
      class MapIterator
      {
      public:
        typedef MapIterator<It, MapFunc> iterator;
        typedef typename MapFunc::result_type value_type;
        MapIterator(It cur, const MapFunc &f) : cur_(cur), func_(f) {}
        value_type operator*() const { return func_(*cur_); }
        MapIterator &operator++()
        {
          ++cur_;
          return *this;
        }
        bool operator!=(const MapIterator &rhs) const { return cur_ != rhs.cur_; }

      private:
        It cur_;
        MapFunc func_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_STREAMVIEW_HPP
