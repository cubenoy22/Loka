#include "core2/scene/NodeManager.hpp"
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/node/ComposableNode.hpp"

namespace
{
  // Recursively compose all ComposableNode in the subtree (pre-order)
  void composeRecursive(declara::core::scene::ComposableNode *node)
  {
    if (!node)
      return;
    node->compose();
    // Access children via INestable to avoid build differences where getChildren isn't found on base
    declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node);
    if (!nestable)
      return;
    const std::vector<declara::core::scene::Node *> &children = nestable->getChildren();
    for (size_t i = 0; i < children.size(); ++i)
    {
      declara::core::scene::ComposableNode *childComposable = dynamic_cast<declara::core::scene::ComposableNode *>(children[i]);
      if (childComposable)
      {
        composeRecursive(childComposable);
      }
    }
  }

  // Recursively materialize all nodes (pre-order)
  void materializeRecursive(declara::core::scene::Node *node, declara::core::scene::IPlatformController *controller)
  {
    if (!node || !controller)
      return;
    // Parent-first materialize
    controller->materialize(node);
    declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node);
    if (!nestable)
      return;
    const std::vector<declara::core::scene::Node *> &children = nestable->getChildren();
    for (size_t i = 0; i < children.size(); ++i)
    {
      materializeRecursive(children[i], controller);
    }
  }
}

namespace declara
{
  namespace core
  {
    namespace scene
    {
      StaticNodeManager::StaticNodeManager()
          : scene_(0), rootNode_(0), platformController_(0)
      {
      }

      StaticNodeManager::~StaticNodeManager()
      {
        unmount();
      }

      bool StaticNodeManager::mount(Scene *scene, IPlatformController *platformController)
      {
        unmount();
        scene_ = scene;
        platformController_ = platformController;
        if (!scene_ || !platformController_)
        {
          return false;
        }

        NodeDefinitionBase *def = scene_->getRootDefinition();
        if (!def)
        {
          return false;
        }

        NodeComposition composition;
        composition.declare(*def);
        rootNode_ = dynamic_cast<ComposableNode *>(composition.createNodeTree());
        if (!rootNode_)
        {
          return false; // ルートがComposableNodeでない場合は失敗
        }
        // 初回compose（Solid.js型）: ルートから全Composableを再帰compose
        composeRecursive(rootNode_);
        // materializeも親→子で再帰
        materializeRecursive(rootNode_, platformController_);
        return true;
      }

      void StaticNodeManager::unmount()
      {
        if (platformController_)
        {
          platformController_->destroy();
          platformController_ = 0;
        }
        clearNodeTree();
        scene_ = 0;
      }

      Node *StaticNodeManager::root() const
      {
        return rootNode_;
      }

      void StaticNodeManager::clearNodeTree()
      {
        if (rootNode_)
        {
          delete rootNode_;
          rootNode_ = 0;
        }
      }

    } // namespace scene
  } // namespace core
} // namespace declara
