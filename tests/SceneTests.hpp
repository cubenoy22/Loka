#ifndef DECLARA_SCENE_TESTS_HPP
#define DECLARA_SCENE_TESTS_HPP

#include <cassert>
#include <stdio.h>
#include "core2/scene/Scene.hpp"
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/node/StaticComposition.hpp"

namespace SceneTests
{
  static int g_rootComposeCount = 0;
  static int g_childComposeCount = 0;

  struct ChildBoundaryTypeTag
  {
  };

  struct ChildBoundaryProps : public declara::core::scene::StaticCompositionProps
  {
    typedef ChildBoundaryTypeTag TypeTag;
  };

  class ChildBoundaryNode : public declara::core::scene::StaticCompositionNode
  {
  public:
    typedef ChildBoundaryTypeTag TypeTag;
    ChildBoundaryNode(const ChildBoundaryProps &p)
        : declara::core::scene::StaticCompositionNode(ChildBoundaryProps(p)) {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      (void)c;
      ++g_childComposeCount;
    }
  };

  struct ChildBoundaryDefinition : public declara::core::scene::BoundaryDefinition<ChildBoundaryProps, ChildBoundaryNode>
  {
    ChildBoundaryDefinition() : BoundaryDefinition<ChildBoundaryProps, ChildBoundaryNode>() {}
  };

  inline ChildBoundaryDefinition ChildBoundary()
  {
    return ChildBoundaryDefinition();
  }

  struct RootBoundaryTypeTag
  {
  };

  struct RootBoundaryProps : public declara::core::scene::StaticCompositionProps
  {
    typedef RootBoundaryTypeTag TypeTag;
  };

  class RootBoundaryNode : public declara::core::scene::StaticCompositionNode
  {
  public:
    typedef RootBoundaryTypeTag TypeTag;
    RootBoundaryNode(const RootBoundaryProps &p)
        : declara::core::scene::StaticCompositionNode(RootBoundaryProps(p)) {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      ++g_rootComposeCount;
      c.declare(ChildBoundary());
    }
  };

  struct RootBoundaryDefinition : public declara::core::scene::BoundaryDefinition<RootBoundaryProps, RootBoundaryNode>
  {
    RootBoundaryDefinition() : BoundaryDefinition<RootBoundaryProps, RootBoundaryNode>() {}
  };

  inline RootBoundaryDefinition RootBoundary()
  {
    return RootBoundaryDefinition();
  }

  void test_Boundary_nested_compose()
  {
    using declara::core::scene::IPlatformController;
    using declara::core::scene::Node;
    using declara::core::scene::Scene;

    class DummyPlatformController : public IPlatformController
    {
    public:
      DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
      virtual void materialize(Node *rootNode)
      {
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

#endif // DECLARA_SCENE_TESTS_HPP
