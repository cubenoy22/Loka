#ifndef LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP
#define LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP

#include "app/scene/boundary/Boundary.hpp"

namespace SceneTestSupport
{
  /** A boundary that re-composes its declaration on UPDATE passes, so tests
      can drive composition diffs (children appearing/retiring) from state
      changes. New tests should use this instead of growing another copy. */
  template <class NodeT, class PropsT>
  class RecomposingBoundaryNode : public loka::app::scene::BoundaryNodeFor<NodeT>
  {
  public:
    explicit RecomposingBoundaryNode(const PropsT &props)
        : loka::app::scene::BoundaryNodeFor<NodeT>(props)
    {
    }

  protected:
    virtual void declareLocalRecomposition(loka::app::scene::NodeComposition &composition)
    {
      this->composeNode(composition);
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::BoundaryNodeFor<NodeT> BaseType;
      if (event != loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        BaseType::composeWithContext(context, event);
        return;
      }

      this->recomposeLocalComposition(context, event, this->LOCAL_RECOMPOSE_APPLY_SNAPSHOT);
    }
  };
} // namespace SceneTestSupport

#endif // LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP
