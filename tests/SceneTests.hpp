#ifndef LOKA_SCENE_TESTS_HPP
#define LOKA_SCENE_TESTS_HPP

#include <cassert>
#include <stdio.h>
#include "app/scene/FlowSlot.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/PlatformController.hpp"
#include "app/scene/nodes/boundary/StdComposition.hpp"

namespace SceneTests
{
  static int g_rootComposeCount = 0;
  static int g_childComposeCount = 0;
  static int g_nodeLocalAttachCount = 0;
  static int g_lateNodeLocalAttachCount = 0;
  static int g_flowSlotProbeLiveCount = 0;
  static int g_flowSlotProbeCopyCount = 0;

  class ChildBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ChildBoundaryNode> ChildBoundaryProps;

  class ChildBoundaryNode : public loka::app::scene::BoundaryNodeFor<ChildBoundaryNode>
  {
  public:
    ChildBoundaryNode(const ChildBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<ChildBoundaryNode>(ChildBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      ++g_childComposeCount;
    }
  };

  inline loka::app::scene::BoundaryDefinition<ChildBoundaryProps, ChildBoundaryNode> ChildBoundary()
  {
    return loka::app::scene::Boundary<ChildBoundaryNode>();
  }

  class RootBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RootBoundaryNode> RootBoundaryProps;

  class RootBoundaryNode : public loka::app::scene::BoundaryNodeFor<RootBoundaryNode>
  {
  public:
    RootBoundaryNode(const RootBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<RootBoundaryNode>(RootBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      ++g_rootComposeCount;
      c.declare(ChildBoundary());
    }
  };

  inline loka::app::scene::BoundaryDefinition<RootBoundaryProps, RootBoundaryNode> RootBoundary()
  {
    return loka::app::scene::Boundary<RootBoundaryNode>();
  }

  void test_Boundary_nested_compose()
  {
    using loka::app::scene::IPlatformController;
    using loka::app::scene::Node;
    using loka::app::scene::Scene;

    class DummyPlatformController : public IPlatformController
    {
    public:
      DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
      virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
      {
        (void)flags;
        lastMaterialized_ = rootNode;
      }
      virtual void synchronize() {}
      virtual bool hasPendingSync() const { return false; }
      virtual void destroy() { destroyed_ = true; }

      Node *lastMaterialized_;
      bool destroyed_;
    };

    g_rootComposeCount = 0;
    g_childComposeCount = 0;
    Scene scene(RootBoundary());
    DummyPlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(g_rootComposeCount == 1);
    assert(g_childComposeCount == 1);
    assert(platform.lastMaterialized_ != 0);
    assert(!platform.destroyed_);

    scene.updateAttached(false);
    assert(platform.destroyed_);

    scene.updateAttached(true);
    assert(g_rootComposeCount == 2);
    assert(g_childComposeCount == 2);

    // Unmount before stack-allocated platform is destroyed
    scene.unmount();
  }

  class FlowSlotProbe
  {
  public:
    FlowSlotProbe(int value)
        : value_(value)
    {
      ++g_flowSlotProbeLiveCount;
    }

    FlowSlotProbe(const FlowSlotProbe &other)
        : value_(other.value_)
    {
      ++g_flowSlotProbeLiveCount;
      ++g_flowSlotProbeCopyCount;
    }

    ~FlowSlotProbe()
    {
      --g_flowSlotProbeLiveCount;
    }

    int value() const { return value_; }

  private:
    int value_;
  };

  void test_FlowSlot_releases_owned_value()
  {
    using loka::app::scene::FlowSlot;

    g_flowSlotProbeLiveCount = 0;
    g_flowSlotProbeCopyCount = 0;
    {
      FlowSlot<FlowSlotProbe> slot;
      assert(!slot.isValid());
      {
        FlowSlotProbe first(3);
        assert(g_flowSlotProbeLiveCount == 1);
        slot.set(first);
        assert(slot.isValid());
        assert(slot.get()->value() == 3);
        assert(g_flowSlotProbeLiveCount == 2);
        assert(g_flowSlotProbeCopyCount == 1);
      }
      assert(g_flowSlotProbeLiveCount == 1);
      {
        FlowSlotProbe second(5);
        slot.set(second);
        assert(slot.isValid());
        assert(slot.get()->value() == 5);
        assert(g_flowSlotProbeLiveCount == 2);
        assert(g_flowSlotProbeCopyCount == 2);
      }
      assert(g_flowSlotProbeLiveCount == 1);
      slot.clear();
      assert(!slot.isValid());
      assert(g_flowSlotProbeLiveCount == 0);
    }
    assert(g_flowSlotProbeLiveCount == 0);
  }

  class NodeLocalStateNode;
  typedef loka::app::scene::BoundaryPropsFor<NodeLocalStateNode> NodeLocalStateProps;

  class NodeLocalStateNode : public loka::app::scene::BoundaryNodeFor<NodeLocalStateNode>
  {
  public:
    NodeLocalStateNode(const NodeLocalStateProps &p)
        : loka::app::scene::BoundaryNodeFor<NodeLocalStateNode>(NodeLocalStateProps(p)),
          count_(),
          attachedOnce_(false)
    {
      this->state(this->count_, 7);
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      assert(this->count_.isValid());
      if (!this->attachedOnce_)
      {
        assert(this->count_.get() == 7);
        this->count_.set(9);
        this->attachedOnce_ = true;
      }
      else
      {
        assert(this->count_.get() == 9);
      }
      ++g_nodeLocalAttachCount;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
    }

  private:
    loka::app::scene::NodeState<int> count_;
    bool attachedOnce_;
  };

  inline loka::app::scene::BoundaryDefinition<NodeLocalStateProps, NodeLocalStateNode> NodeLocalStateBoundary()
  {
    return loka::app::scene::Boundary<NodeLocalStateNode>();
  }

  void test_Node_local_state_registration_is_idempotent()
  {
    using loka::app::scene::IPlatformController;
    using loka::app::scene::Node;
    using loka::app::scene::Scene;

    class DummyPlatformController : public IPlatformController
    {
    public:
      DummyPlatformController() : destroyed_(false) {}
      virtual void onChange(Node *, loka::app::scene::NodeDirtyFlags, bool) {}
      virtual void synchronize() {}
      virtual bool hasPendingSync() const { return false; }
      virtual void destroy() { destroyed_ = true; }

      bool destroyed_;
    };

    g_nodeLocalAttachCount = 0;
    Scene scene(NodeLocalStateBoundary());
    DummyPlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(g_nodeLocalAttachCount == 1);

    scene.updateAttached(false);
    assert(platform.destroyed_);

    scene.updateAttached(true);
    assert(g_nodeLocalAttachCount == 2);

    scene.unmount();
  }

  class LateNodeLocalStateNode;
  typedef loka::app::scene::BoundaryPropsFor<LateNodeLocalStateNode> LateNodeLocalStateProps;

  class LateNodeLocalStateNode : public loka::app::scene::BoundaryNodeFor<LateNodeLocalStateNode>
  {
  public:
    LateNodeLocalStateNode(const LateNodeLocalStateProps &p)
        : loka::app::scene::BoundaryNodeFor<LateNodeLocalStateNode>(LateNodeLocalStateProps(p)),
          late_(),
          registered_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      if (!this->registered_)
      {
        this->state(this->late_, 11);
        this->registered_ = true;
      }
      assert(this->late_.isValid());
      assert(this->late_.get() == 11);
      ++g_lateNodeLocalAttachCount;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
    }

  private:
    loka::app::scene::NodeState<int> late_;
    bool registered_;
  };

  inline loka::app::scene::BoundaryDefinition<LateNodeLocalStateProps, LateNodeLocalStateNode> LateNodeLocalStateBoundary()
  {
    return loka::app::scene::Boundary<LateNodeLocalStateNode>();
  }

  void test_Node_local_state_registration_after_attach_connects_immediately()
  {
    using loka::app::scene::IPlatformController;
    using loka::app::scene::Node;
    using loka::app::scene::Scene;

    class DummyPlatformController : public IPlatformController
    {
    public:
      virtual void onChange(Node *, loka::app::scene::NodeDirtyFlags, bool) {}
      virtual void synchronize() {}
      virtual bool hasPendingSync() const { return false; }
      virtual void destroy() {}
    };

    g_lateNodeLocalAttachCount = 0;
    Scene scene(LateNodeLocalStateBoundary());
    DummyPlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(g_lateNodeLocalAttachCount == 1);
    scene.unmount();
  }

  typedef void (*TestFunc)();

  void runAll()
  {
    TestFunc tests[] = {
        test_Boundary_nested_compose,
        test_FlowSlot_releases_owned_value,
        test_Node_local_state_registration_is_idempotent,
        test_Node_local_state_registration_after_attach_connects_immediately};
    const int numTests = sizeof(tests) / sizeof(tests[0]);
    for (int i = 0; i < numTests; ++i)
    {
      tests[i]();
    }
    printf("SceneTests: All tests passed!\n");
  }
}

#endif // LOKA_SCENE_TESTS_HPP
