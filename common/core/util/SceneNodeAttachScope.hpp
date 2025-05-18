#ifndef DECLARA_SCENENODEATTACHSCOPE_HPP
#define DECLARA_SCENENODEATTACHSCOPE_HPP

// 前方宣言で循環依存を回避
class SceneNodeGroup;
class LayoutSceneNode;

#include <vector>

struct AttachTarget
{
  enum Type
  {
    Group,
    Layout
  } type;
  void *ptr;
};
extern std::vector<AttachTarget> attachTargetStack;

class SceneNodeAttachScope
{
public:
  SceneNodeAttachScope(AttachTarget::Type type, void *ptr)
  {
    attachTargetStack.push_back({type, ptr});
  }
  ~SceneNodeAttachScope()
  {
    attachTargetStack.pop_back();
  }
  static AttachTarget current()
  {
    return attachTargetStack.empty() ? AttachTarget{AttachTarget::Group, nullptr} : attachTargetStack.back();
  }
  static void autoAttach(SceneNode *node);
};

#endif // DECLARA_SCENENODEATTACHSCOPE_HPP
