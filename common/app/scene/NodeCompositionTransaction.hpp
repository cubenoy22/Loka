#ifndef LOKA_CORE2_SCENE_NODECOMPOSITIONTRANSACTION_HPP
#define LOKA_CORE2_SCENE_NODECOMPOSITIONTRANSACTION_HPP

#include "app/scene/NodeCompositionCompare.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct NodeComposition;

      struct NodeCompositionTransaction
      {
        NodeCompositionTransaction()
            : previous_(0),
              current_(0),
              retiredCount_(0),
              diff_()
        {
        }

        void begin(NodeComposition *previous, NodeComposition *current)
        {
          this->previous_ = previous;
          this->current_ = current;
          this->retiredCount_ = 0;
          this->diff_.clear();
        }

        void clear()
        {
          this->previous_ = 0;
          this->current_ = 0;
          this->retiredCount_ = 0;
          this->diff_.clear();
        }

        bool empty() const
        {
          return this->previous_ == 0 && this->current_ == 0 && this->retiredCount_ == 0 && this->diff_.empty();
        }

        NodeComposition *previous() const { return this->previous_; }
        NodeComposition *current() const { return this->current_; }

        NodeCompositionDiff &diff() { return this->diff_; }
        const NodeCompositionDiff &diff() const { return this->diff_; }

        bool buildDiffByTag()
        {
          if (!this->previous_ || !this->current_)
          {
            this->diff_.clear();
            return false;
          }
          return buildNodeCompositionDiffByTag(*this->previous_, *this->current_, this->diff_);
        }

        void noteRetiredChild()
        {
          this->retiredCount_ += 1;
        }

        size_t retiredCount() const { return this->retiredCount_; }

      private:
        NodeComposition *previous_;
        NodeComposition *current_;
        size_t retiredCount_;
        NodeCompositionDiff diff_;
      };
    }
  }
}

#endif // LOKA_CORE2_SCENE_NODECOMPOSITIONTRANSACTION_HPP
