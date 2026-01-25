#ifndef LOKA_SCENE_TESTS_HPP
#define LOKA_SCENE_TESTS_HPP

#include <cassert>
#include <stdio.h>
#include "core2/scene/Scene.hpp"
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/node/StaticComposition.hpp"

namespace SceneTests
{
  static int g_rootComposeCount = 0;
  static int g_childComposeCount = 0;

  class ChildBoundaryNode;
  typedef loka::core::scene::BoundaryPropsFor<ChildBoundaryNode> ChildBoundaryProps;

  class ChildBoundaryNode : public loka::core::scene::BoundaryNodeFor<ChildBoundaryNode>
  {
  public:
    ChildBoundaryNode(const ChildBoundaryProps &p)
        : loka::core::scene::BoundaryNodeFor<ChildBoundaryNode>(ChildBoundaryProps(p)) {}

    virtual void composeNode(loka::core::scene::NodeComposition &c)
    {
      (void)c;
      ++g_childComposeCount;
    }
  };

  inline loka::core::scene::BoundaryDefinition<ChildBoundaryProps, ChildBoundaryNode> ChildBoundary()
  {
    return loka::core::scene::Boundary<ChildBoundaryNode>();
  }

  class RootBoundaryNode;
  typedef loka::core::scene::BoundaryPropsFor<RootBoundaryNode> RootBoundaryProps;

  class RootBoundaryNode : public loka::core::scene::BoundaryNodeFor<RootBoundaryNode>
  {
  public:
    RootBoundaryNode(const RootBoundaryProps &p)
        : loka::core::scene::BoundaryNodeFor<RootBoundaryNode>(RootBoundaryProps(p)) {}

    virtual void composeNode(loka::core::scene::NodeComposition &c)
    {
      ++g_rootComposeCount;
      c.declare(ChildBoundary());
    }
  };

  inline loka::core::scene::BoundaryDefinition<RootBoundaryProps, RootBoundaryNode> RootBoundary()
  {
    return loka::core::scene::Boundary<RootBoundaryNode>();
  }

  void test_Boundary_nested_compose()
  {
    using loka::core::scene::IPlatformController;
    using loka::core::scene::Node;
    using loka::core::scene::Scene;

    class DummyPlatformController : public IPlatformController
    {
    public:
      DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
      virtual void onChange(Node *rootNode, loka::core::scene::NodeDirtyFlags flags)
      {
        (void)flags;
        lastMaterialized_ = rootNode;
      }
      virtual void synchronize() {}
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

  typedef void (*TestFunc)();

  void runAll()
  {
    TestFunc tests[] = {
        test_Boundary_nested_compose};
    const int numTests = sizeof(tests) / sizeof(tests[0]);
    for (int i = 0; i < numTests; ++i)
    {
      tests[i]();
    }
    printf("SceneTests: All tests passed!\n");
  }
}

#endif // LOKA_SCENE_TESTS_HPP
