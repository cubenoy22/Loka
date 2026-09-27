#ifndef LOKA_TESTS_SUPPORT_OBSERVED_USE_COUNTS_HPP
#define LOKA_TESTS_SUPPORT_OBSERVED_USE_COUNTS_HPP
#include "testing/scene/NodeObservedUsesTestAccess.hpp"
namespace ObservedUseTestSupport
{
  inline unsigned activeUses(loka::app::scene::Node *node)
  {
    if (!node) return 0;
    unsigned count = loka::app::testing::NodeObservedUsesTestAccess::useCount(*node);
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
         child; child = child->nextInComposition)
      count += activeUses(child);
    return count;
  }
}
#endif
