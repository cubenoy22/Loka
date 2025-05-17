#ifndef DECLARA_SCENENODEATTACHSCOPE_HPP
#define DECLARA_SCENENODEATTACHSCOPE_HPP

#include "core/SceneNodeGroup.hpp"
#include "core/LayoutSceneNode.hpp"
#include <vector>

struct AttachTarget {
  enum Type { Group, Layout } type;
  void* ptr;
};
static std::vector<AttachTarget> attachTargetStack;

class SceneNodeAttachScope {
public:
  SceneNodeAttachScope(AttachTarget::Type type, void* ptr) {
    attachTargetStack.push_back({type, ptr});
  }
  ~SceneNodeAttachScope() {
    attachTargetStack.pop_back();
  }
  static AttachTarget current() {
    return attachTargetStack.empty() ? AttachTarget{AttachTarget::Group, nullptr} : attachTargetStack.back();
  }
  static void autoAttach(SceneNode* node) {
    AttachTarget t = current();
    if (t.ptr) {
      if (t.type == AttachTarget::Group) {
        static_cast<SceneNodeGroup*>(t.ptr)->add(node);
      } else if (t.type == AttachTarget::Layout) {
        static_cast<LayoutSceneNode*>(t.ptr)->addChild(node);
      }
    }
  }
};

#endif // DECLARA_SCENENODEATTACHSCOPE_HPP
