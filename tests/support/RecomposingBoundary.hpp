#ifndef LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP
#define LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP

#include "app/scene/boundary/Boundary.hpp"

namespace SceneTestSupport
{
  /** A boundary that re-composes its declaration on UPDATE passes, so tests
      can drive composition diffs (children appearing/retiring) from state
      changes. Set UseRetainFastPaths when the test must exercise retained
      props application. New tests should use this instead of growing another
      copy. */
  template <class NodeT, class PropsT, bool UseRetainFastPaths = false>
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

      this->recomposeLocalComposition(
          context,
          event,
          UseRetainFastPaths
              ? this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS
              : this->LOCAL_RECOMPOSE_APPLY_SNAPSHOT);
    }
  };
} // namespace SceneTestSupport

#endif // LOKA_TESTS_SUPPORT_RECOMPOSING_BOUNDARY_HPP
