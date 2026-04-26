#ifndef LOKA_SCENE_TESTS_HPP
#define LOKA_SCENE_TESTS_HPP

#include <cassert>
#include <stdio.h>
#include "app/scene/Scene.hpp"
#include "app/scene/PlatformController.hpp"
#include "app/scene/nodes/boundary/StdComposition.hpp"

namespace SceneTests
{
  static int g_rootComposeCount = 0;
  static int g_childComposeCount = 0;
  static int g_nodeLocalAttachCount = 0;

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
    loka::app::scene::BoundState<int> count_;
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

  typedef void (*TestFunc)();

  void runAll()
  {
    TestFunc tests[] = {
        test_Boundary_nested_compose,
        test_Node_local_state_registration_is_idempotent};
    const int numTests = sizeof(tests) / sizeof(tests[0]);
    for (int i = 0; i < numTests; ++i)
    {
      tests[i]();
    }
    printf("SceneTests: All tests passed!\n");
  }
}

#endif // LOKA_SCENE_TESTS_HPP
