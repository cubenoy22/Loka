#ifndef LOKA_TESTS_SUPPORT_LIFECYCLE_FACT_TEST_ACCESS_HPP
#define LOKA_TESTS_SUPPORT_LIFECYCLE_FACT_TEST_ACCESS_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Test backdoor: runs the apply-phase fact-delivery walk on a bare
          node tree, so unit pins can observe deliveries without mounting a
          Scene. Scene-level placement (delivery at the head of apply, and
          nowhere else) is pinned by the scene-based tests instead. */
      struct LifecycleFactTestAccess
      {
        /** Runs the seat-evaluation portion of a compose walk on a bare live
            tree before a unit pin drives apply-phase fact delivery. */
        static void EvaluateConditionalSeats(Node *root)
        {
          if (!root)
          {
            return;
          }
          Node::EvaluateChildrenForScheduledApplySubtree(root);
        }

        static void DeliverFacts(Node *root)
        {
          Node::DeliverLifecycleFactsSubtree(root);
        }

        /** Drives the retire door on a bare tree (what retireDetachedNode
            and teardown do before contexts are peeled). */
        static void MarkSubtreeRetired(Node *root)
        {
          Node::MarkSubtreeLifecycleFact(root, NODE_FACT_RETIRED);
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_TESTS_SUPPORT_LIFECYCLE_FACT_TEST_ACCESS_HPP
