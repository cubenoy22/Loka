#include "core2/scene/NodeManager.hpp"
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/ComponentContext.hpp"
#include "core2/scene/node/ComposableNode.hpp"

namespace
{
  void composeRecursive(declara::core::scene::ComposableNode *node, declara::core::scene::ComponentContext &context);
  void composeFromNode(declara::core::scene::Node *node, declara::core::scene::ComponentContext &context);

  void traverseChildren(const std::vector<declara::core::scene::Node *> &children, declara::core::scene::ComponentContext &context)
  {
    for (size_t i = 0; i < children.size(); ++i)
    {
      declara::core::scene::Node *child = children[i];
      declara::core::scene::ComposableNode *childComposable = dynamic_cast<declara::core::scene::ComposableNode *>(child);
      if (childComposable)
      {
        declara::core::scene::ComponentContext childContext(&context);
        composeRecursive(childComposable, childContext);
        continue;
      }
      declara::core::scene::INestable *childNestable = dynamic_cast<declara::core::scene::INestable *>(child);
      if (childNestable)
      {
        traverseChildren(childNestable->getChildren(), context);
      }
    }
  }

  // Recursively compose all ComposableNode in the subtree (pre-order)
  void composeRecursive(declara::core::scene::ComposableNode *node, declara::core::scene::ComponentContext &context)
  {
    if (!node)
      return;
    node->compose(context);
    // Access children via INestable to avoid build differences where getChildren isn't found on base
    declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node);
    if (!nestable)
      return;
    const std::vector<declara::core::scene::Node *> &children = nestable->getChildren();
    traverseChildren(children, context);
  }

  void composeFromNode(declara::core::scene::Node *node, declara::core::scene::ComponentContext &context)
  {
    if (!node)
      return;
    declara::core::scene::ComposableNode *composable = dynamic_cast<declara::core::scene::ComposableNode *>(node);
    if (composable)
    {
      composeRecursive(composable, context);
      return;
    }
    declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node);
    if (nestable)
    {
      const std::vector<declara::core::scene::Node *> &children = nestable->getChildren();
      traverseChildren(children, context);
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
        rootNode_ = composition.createNodeTree();
        if (!rootNode_)
        {
          return false;
        }
        // 初回compose（Solid.js型）: ルートから全Composableを再帰compose
        ComponentContext rootContext;
        composeFromNode(rootNode_, rootContext);
        // materializeはルートからプラットフォームコントローラーに委譲
        platformController_->materialize(rootNode_);
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
