#include "core2/scene/NodeManager.hpp"
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/node/ComposableNode.hpp"

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
        // 初回compose（Solid.js型）
        rootNode_->compose();
        if (rootNode_)
        {
          platformController_->materialize(rootNode_);
          return true;
        }
        return false;
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
