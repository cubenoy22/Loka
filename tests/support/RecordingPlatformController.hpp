#ifndef LOKA_TESTS_SUPPORT_RECORDING_PLATFORM_CONTROLLER_HPP
#define LOKA_TESTS_SUPPORT_RECORDING_PLATFORM_CONTROLLER_HPP

#include <cassert>
#include <vector>
#include "app/scene/projection/PlatformController.hpp"

namespace SceneTestSupport
{
  /** One ordered platform notification emitted by a Scene. */
  struct PlatformChangeRecord
  {
    PlatformChangeRecord(loka::app::scene::Node *changedNode,
                         loka::app::scene::NodeDirtyFlags dirtyFlags,
                         bool rebuild,
                         unsigned long order)
        : node(changedNode),
          flags(dirtyFlags),
          fullRebuild(rebuild),
          callOrder(order)
    {
    }

    loka::app::scene::Node *node;
    loka::app::scene::NodeDirtyFlags flags;
    bool fullRebuild;
    unsigned long callOrder;
  };

  /** Test platform controller that preserves the complete Scene notification sequence. */
  class RecordingPlatformController : public loka::app::scene::IPlatformController
  {
  public:
    typedef std::vector<PlatformChangeRecord> ChangeSequence;

    RecordingPlatformController()
        : lastMaterialized_(0),
          destroyed_(false),
          changes_()
    {
    }

    virtual void onChange(loka::app::scene::Node *rootNode,
                          loka::app::scene::NodeDirtyFlags flags,
                          bool fullRebuild)
    {
      this->changes_.push_back(PlatformChangeRecord(
          rootNode, flags, fullRebuild, static_cast<unsigned long>(this->changes_.size())));
      this->lastMaterialized_ = rootNode;
    }

    virtual void synchronize() {}

    virtual bool hasPendingSync() const
    {
      return false;
    }

    virtual void destroy()
    {
      this->destroyed_ = true;
    }

    void clearChanges()
    {
      this->changes_.clear();
    }

    size_t changeCount() const
    {
      return this->changes_.size();
    }

    size_t changeCountForNode(const loka::app::scene::Node *node) const
    {
      size_t count = 0;
      for (size_t i = 0; i < this->changes_.size(); ++i)
      {
        if (this->changes_[i].node == node)
        {
          ++count;
        }
      }
      return count;
    }

    loka::app::scene::NodeDirtyFlags flagsSeenForNode(const loka::app::scene::Node *node) const
    {
      int flags = loka::app::scene::NODE_DIRTY_NONE;
      for (size_t i = 0; i < this->changes_.size(); ++i)
      {
        if (this->changes_[i].node == node)
        {
          flags |= this->changes_[i].flags;
        }
      }
      return static_cast<loka::app::scene::NodeDirtyFlags>(flags);
    }

    const PlatformChangeRecord &changeAt(size_t index) const
    {
      assert(index < this->changes_.size());
      return this->changes_[index];
    }

    const ChangeSequence &changeSequence() const
    {
      return this->changes_;
    }

    loka::app::scene::Node *lastMaterialized_;
    bool destroyed_;

  private:
    ChangeSequence changes_;
  };
} // namespace SceneTestSupport

#endif // LOKA_TESTS_SUPPORT_RECORDING_PLATFORM_CONTROLLER_HPP
