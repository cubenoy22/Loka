#ifndef LOKA_DSL_COMPOSITION_LIST_HPP
#define LOKA_DSL_COMPOSITION_LIST_HPP

#include <vector>

namespace loka
{
  namespace dsl
  {
    template <typename DefT>
    class CompositionList
    {
    public:
      CompositionList() : head_(0), tail_(0), count_(0) {}
      ~CompositionList() { clear(); }

      size_t count() const { return count_; }

      void appendClone(const DefT &def)
      {
        DefT *node = def.clone();
        appendOwned(node);
      }

      void appendOwned(DefT *node)
      {
        if (!node)
          return;
        node->nextInComposition = 0;
        if (!head_)
        {
          head_ = node;
          tail_ = node;
        }
        else
        {
          tail_->nextInComposition = node;
          tail_ = node;
        }
        count_ += 1;
      }

      void detachTo(std::vector<DefT *> &out)
      {
        out.clear();
        out.reserve(count_);
        while (head_)
        {
          DefT *next = head_->nextInComposition;
          head_->nextInComposition = 0;
          out.push_back(head_);
          head_ = next;
        }
        tail_ = 0;
        count_ = 0;
      }

      bool remove(DefT *node)
      {
        if (!node)
          return false;
        DefT *prev = 0;
        DefT *cur = head_;
        while (cur)
        {
          if (cur == node)
          {
            if (prev)
            {
              prev->nextInComposition = cur->nextInComposition;
            }
            else
            {
              head_ = cur->nextInComposition;
            }
            if (tail_ == cur)
            {
              tail_ = prev;
            }
            cur->nextInComposition = 0;
            if (count_ > 0)
            {
              count_ -= 1;
            }
            return true;
          }
          prev = cur;
          cur = cur->nextInComposition;
        }
        return false;
      }

      void clear()
      {
        while (head_)
        {
          DefT *next = head_->nextInComposition;
          delete head_;
          head_ = next;
        }
        tail_ = 0;
        count_ = 0;
      }

    private:
      DefT *head_;
      DefT *tail_;
      size_t count_;
    };
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_COMPOSITION_LIST_HPP
