#ifndef LOKA_LAZYLIST_MODEL_HPP
#define LOKA_LAZYLIST_MODEL_HPP

#include "CardNode.hpp"
#include "core/ObservableList.hpp"
#include "core/Frame.hpp"
#include <climits>

#ifndef LAZYLIST_CAPACITY
#define LAZYLIST_CAPACITY 100
#endif
#if LAZYLIST_CAPACITY < 1 || LAZYLIST_CAPACITY > 1000
#error LAZYLIST_CAPACITY must be between 1 and 1000
#endif

namespace lazylist
{
  enum
  {
    kWindowWidth = 640,
    kMargin = 8,
    kCellWidth = kWindowWidth - 2 * kMargin,
    kCellHeight = 20,
    kViewportHeight = 8 * kCellHeight,
    kWindowHeight = 244
  };

  /** App-owned cards and content-coordinate viewport; destroy views before this model. */
  class LazyListModel
  {
  private:
    loka::core::PushStateTracker tracker_;

  public:
    LazyListModel()
        : tracker_(),
          cards(),
          viewport_(loka::core::Frame(0, 0, kCellWidth, kViewportHeight)),
          attachment_(this->cards.attach(&this->tracker_, LAZYLIST_CAPACITY))
    {
      this->tracker_.addState(&this->viewport_);
      if (this->attachment_ != loka::core::ATTACH_OK)
        return;
      for (short i = 0; i < LAZYLIST_CAPACITY; ++i)
      {
        if (this->cards.insert(i, CardProps(cardLabel(i), i)) != loka::core::EDIT_OK)
          break;
      }
    }
    ~LazyListModel()
    {
      this->tracker_.removeState(&this->viewport_);
    }
    loka::core::ListAttachResult attachment() const
    {
      return this->attachment_;
    }
    loka::core::ListEditResult nextPage()
    {
      return this->page(this->viewport_.get().height);
    }
    loka::core::ListEditResult prevPage()
    {
      return this->page(-this->viewport_.get().height);
    }

    loka::core::ListEditResult renameCard(unsigned short index)
    {
      if (index >= this->cards.size())
        return loka::core::EDIT_INDEX_OUT_OF_RANGE;
      const loka::core::ObservableList<CardProps>::Entry &entry = this->cards.at(index);
      if (entry.value.renameCount == SHRT_MAX)
        return loka::core::EDIT_SEQ_EXHAUSTED;
      const short count = static_cast<short>(entry.value.renameCount + 1);
      return this->cards.update(entry.id,
                                CardProps(cardLabel(entry.value.number) + loka::core::String::Literal(" (")
                                              + loka::core::String::FromInt(count) + loka::core::String::Literal(")"),
                                          entry.value.number,
                                          count));
    }
    loka::core::ListEditResult removeFirst()
    {
      const loka::core::ListEditResult result =
          this->cards.remove(this->cards.size() ? this->cards.at(0).id : loka::core::ItemId::none());
      if (result == loka::core::EDIT_OK)
        this->page(0);
      return result;
    }
    loka::core::ListEditResult insertAtTop()
    {
      // A fresh display number comes from the completed list, not another mutable sequence.
      int number = 0;
      for (unsigned short i = 0; i < this->cards.size(); ++i)
        if (this->cards.at(i).value.number >= number)
          number = this->cards.at(i).value.number + 1;
      if (number > SHRT_MAX)
        return loka::core::EDIT_SEQ_EXHAUSTED;
      return this->cards.insert(0, CardProps(cardLabel(number), static_cast<short>(number)));
    }
    loka::core::ListEditResult moveFirstToEnd()
    {
      return this->cards.move(this->cards.size() ? this->cards.at(0).id : loka::core::ItemId::none(),
                              this->cards.size() ? this->cards.size() - 1 : 0);
    }

    loka::core::ObservableList<CardProps> cards;
    loka::core::State<loka::core::Frame> &viewport()
    {
      return this->viewport_;
    }

  private:
    LazyListModel(const LazyListModel &);
    LazyListModel &operator=(const LazyListModel &);
    static loka::core::String cardLabel(int number)
    {
      return loka::core::String::Literal("Card ") + loka::core::String::FromInt(number);
    }
    loka::core::ListEditResult page(int delta)
    {
      if (this->attachment_ != loka::core::ATTACH_OK)
        return loka::core::EDIT_NOT_ATTACHED;
      loka::core::Frame view = this->viewport_.get();
      const int extent = this->cards.size() * kCellHeight - view.height;
      const int maximum = extent > 0 ? extent : 0;
      const int next = view.y + delta;
      view.y = next < 0 ? 0 : (next > maximum ? maximum : next);
      if (view != this->viewport_.get())
      {
        loka::core::StateTrackerGuard guard(&this->tracker_);
        this->viewport_.set(view);
      }
      return loka::core::EDIT_OK;
    }
    loka::core::MutableState<loka::core::Frame> viewport_;
    const loka::core::ListAttachResult attachment_;
  };
} // namespace lazylist
#endif
