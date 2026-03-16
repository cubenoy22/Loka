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
      struct NodeCompositionSnapshot;

      struct NodeCompositionTransaction
      {
        NodeCompositionTransaction()
            : previous_(0),
              current_(0),
              previousSnapshot_(0),
              currentSnapshot_(0),
              retiredCount_(0),
              diff_()
        {
        }

        void begin(NodeComposition *previous, NodeComposition *current)
        {
          this->previous_ = previous;
          this->current_ = current;
          this->previousSnapshot_ = 0;
          this->currentSnapshot_ = 0;
          this->retiredCount_ = 0;
          this->diff_.clear();
        }

        void beginSnapshots(NodeCompositionSnapshot *previous, NodeCompositionSnapshot *current)
        {
          this->previous_ = 0;
          this->current_ = 0;
          this->previousSnapshot_ = previous;
          this->currentSnapshot_ = current;
          this->retiredCount_ = 0;
          this->diff_.clear();
        }

        void clear()
        {
          this->previous_ = 0;
          this->current_ = 0;
          this->previousSnapshot_ = 0;
          this->currentSnapshot_ = 0;
          this->retiredCount_ = 0;
          this->diff_.clear();
        }

        bool empty() const
        {
          return this->previous_ == 0 && this->current_ == 0 &&
                 this->previousSnapshot_ == 0 && this->currentSnapshot_ == 0 &&
                 this->retiredCount_ == 0 && this->diff_.empty();
        }

        NodeComposition *previous() const { return this->previous_; }
        NodeComposition *current() const { return this->current_; }
        NodeCompositionSnapshot *previousSnapshot() const { return this->previousSnapshot_; }
        NodeCompositionSnapshot *currentSnapshot() const { return this->currentSnapshot_; }

        NodeCompositionDiff &diff() { return this->diff_; }
        const NodeCompositionDiff &diff() const { return this->diff_; }

        bool buildDiffByTag()
        {
          if (this->previous_ && this->current_)
          {
            return buildNodeCompositionDiffByTag(*this->previous_, *this->current_, this->diff_);
          }
          if (this->previousSnapshot_ && this->currentSnapshot_)
          {
            return buildNodeCompositionDiffByTag(*this->previousSnapshot_, *this->currentSnapshot_, this->diff_);
          }
          this->diff_.clear();
          return false;
        }

        void noteRetiredChild()
        {
          this->retiredCount_ += 1;
        }

        size_t retiredCount() const { return this->retiredCount_; }

      private:
        NodeComposition *previous_;
        NodeComposition *current_;
        NodeCompositionSnapshot *previousSnapshot_;
        NodeCompositionSnapshot *currentSnapshot_;
        size_t retiredCount_;
        NodeCompositionDiff diff_;
      };
    }
  }
}

#endif // LOKA_CORE2_SCENE_NODECOMPOSITIONTRANSACTION_HPP
