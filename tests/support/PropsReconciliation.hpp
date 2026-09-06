#ifndef LOKA_TESTS_SUPPORT_PROPS_RECONCILIATION_HPP
#define LOKA_TESTS_SUPPORT_PROPS_RECONCILIATION_HPP

#include "RecomposingBoundary.hpp"
#include "TestVerify.hpp"
#include "app/scene/Scene.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace PropsReconciliationSupport
{
  template <class Definition> class Tree;

  /** Borrows the test's declaration, which outlives the mounted scene. */
  template <class Definition> struct Props : loka::app::scene::NodePropsBase<Props<Definition> >
  {
    typedef Tree<Definition> NodeType;
    struct TypeTag
    {
    };
    explicit Props(const Definition *value = 0)
        : definition(value)
    {
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return this->definition < static_cast<const Props &>(rhs).definition;
    }
    const Definition *definition;
  };

  template <class Definition>
  class Tree : public SceneTestSupport::RecomposingBoundaryNode<
                   Tree<Definition>,
                   Props<Definition>,
                   true,
                   loka::app::scene::StdCompositionBoundaryNodeBase<Props<Definition> > >
  {
    typedef SceneTestSupport::RecomposingBoundaryNode<
        Tree<Definition>,
        Props<Definition>,
        true,
        loka::app::scene::StdCompositionBoundaryNodeBase<Props<Definition> > >
        Base;

  public:
    explicit Tree(const Props<Definition> &props)
        : Base(props)
    {
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(*this->props.definition);
    }
    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }
  };

  inline void settle(loka::app::scene::Scene &scene)
  {
    for (int i = 0; scene.hasPendingInvalidation() && i < 12; ++i)
      LOKA_VERIFY(scene.flushInvalidation());
    assert(!scene.hasPendingInvalidation());
  }

  inline loka::app::scene::BoundaryNode *root(loka::app::scene::Scene &scene)
  {
    return loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  }

  inline void recompose(loka::app::scene::Scene &scene)
  {
    root(scene)->markViewDirty(loka::app::scene::NODE_DIRTY_PROPS);
    settle(scene);
  }
} // namespace PropsReconciliationSupport
#endif
