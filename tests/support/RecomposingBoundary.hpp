#ifndef LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP
#define LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP

#include <cstddef>
#include <vector>

#include "app/scene/boundary/Boundary.hpp"

namespace SceneTestSupport
{
  /** A boundary that re-composes its declared children on UPDATE passes,
      so tests can drive composition diffs (children appearing/retiring)
      from state changes. Same shape as the file-local helpers in
      NullPlatformContractTests.cpp / LifecycleDetachTests.cpp — new tests
      should include this one instead of growing another copy. */
  template <class NodeT, class PropsT>
  class RecomposingBoundaryNode : public loka::app::scene::BoundaryNodeFor<NodeT>
  {
  public:
    explicit RecomposingBoundaryNode(const PropsT &props)
        : loka::app::scene::BoundaryNodeFor<NodeT>(props)
    {
    }

  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::BoundaryNodeFor<NodeT> BaseType;
      if (event != loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        BaseType::composeWithContext(context, event);
        return;
      }

      loka::app::scene::NodeComposition &composition = this->beginComposition(context);
      {
        loka::app::scene::NodeComposition::CompositionScope scope(composition);
        this->composeNode(composition);
      }
      this->captureCurrentCompositionSnapshot();
      this->rebuildCurrentCompositionDiff();
      std::vector<loka::app::scene::Node *> retainedChildren;
      if (!this->rebuildCompositionChildrenFromCurrentSnapshot(context, retainedChildren))
      {
        return;
      }
      this->promoteCurrentCompositionSnapshot();
      for (std::size_t i = 0; i < retainedChildren.size(); ++i)
      {
        if (retainedChildren[i])
        {
          this->composeSubtree(retainedChildren[i], context, event, this);
        }
      }
    }
  };
} // namespace SceneTestSupport

#endif // LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP
