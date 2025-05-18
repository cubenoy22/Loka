#include "core/SceneNode.hpp"
#include "core/util/SceneNodeAttachScope.hpp"
#include "core/SceneNodeGroup.hpp"
#include "core/LayoutSceneNode.hpp"

std::vector<AttachTarget> attachTargetStack;

#include <cstdio>

void SceneNodeAttachScope::autoAttach(SceneNode *node)
{
  AttachTarget t = current();
  // Debug output
  printf("[autoAttach] node=%p type=%s target=%p\n", node, t.type == AttachTarget::Group ? "Group" : "Layout", t.ptr);
  if (t.ptr)
  {
    if (t.type == AttachTarget::Group)
    {
      static_cast<SceneNodeGroup *>(t.ptr)->add(node);
    }
    else if (t.type == AttachTarget::Layout)
    {
      static_cast<LayoutSceneNode *>(t.ptr)->addChild(node);
    }
  }
}
