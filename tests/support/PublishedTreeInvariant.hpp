#ifndef LOKA_TESTS_SUPPORT_PUBLISHED_TREE_INVARIANT_HPP
#define LOKA_TESTS_SUPPORT_PUBLISHED_TREE_INVARIANT_HPP

#include "app/scene/Node.hpp"
#include <cstdio>
#include <vector>

namespace SceneTestSupport
{
  /** Snapshot tagged declaration members during publication, while the tree is
      alive. Keeping node pointers here would create a dangling reference after
      replacement; the observer owns only copied value facts. */
  inline void CollectPublishedTags(loka::app::scene::Node *node, std::vector<loka::app::scene::NodeTag> &tags)
  {
    if (!node)
      return;
    if (node->nodeTag() != loka::app::scene::NODE_TAG_NONE)
      tags.push_back(node->nodeTag());
    loka::app::scene::INestable *children = node->asNestable();
    if (children)
      for (loka::app::scene::Node *child = children->childrenHead(); child; child = child->nextInComposition)
        CollectPublishedTags(child, tags);
  }

  /** Absolute publication contract, shared by refusal and healthy pins.
      Callers capture tags by walking the published tree at onChange. Returning
      the verdict lets a pin observe all external retries before failing. */
  inline bool PublishedTreeMatchesDeclarationOrWhiteFlag(const std::vector<loka::app::scene::NodeTag> &published,
                                                         const std::vector<loka::app::scene::NodeTag> &declared,
                                                         bool whiteFlag,
                                                         const char *stage)
  {
    const bool invariant = published.size() == declared.size() || whiteFlag;
    const bool order = published.size() != declared.size() || published == declared;
    if (!invariant || !order)
      std::fprintf(stderr,
                   "%s: published child count == declared count OR white flag armed: "
                   "%lu/%lu white=%d order=%d FAILED\n",
                   stage,
                   static_cast<unsigned long>(published.size()),
                   static_cast<unsigned long>(declared.size()),
                   whiteFlag,
                   order);
    return invariant && order;
  }
} // namespace SceneTestSupport
#endif
