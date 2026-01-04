#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/Node.hpp"
#include "core2/scene/Scene.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "core/Window.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {

      // 宣言のみ。実際の型はNode.hppで定義されるINestableなどから取得
      struct INestableDefinition;
      struct INestable;

      // createNodeTreeの再帰ヘルパー
      static Node *createNodeRecursive(const NodeDefinitionBase *def)
      {
        if (!def)
        {
          return 0;
        }

        // 1. Nodeインスタンスを生成
        Node *node = def->create();

        // 2. 子を持つことができるかチェック
        const INestableDefinition *nestableDef = dynamic_cast<const INestableDefinition *>(def);
        INestable *nestableNode = dynamic_cast<INestable *>(node);

        if (nestableDef && nestableNode)
        {
          const std::vector<NodeDefinitionBase *> &children = nestableDef->getChildren();
          for (size_t i = 0; i < children.size(); ++i)
          {
            Node *childNode = createNodeRecursive(children[i]);
            if (childNode)
            {
              nestableNode->addChild(childNode);
            }
          }
        }

        return node;
      }

      Node *NodeComposition::createNodeTree() const
      {
        return createNodeRecursive(this->root());
      }

      BoundaryNode *NodeComposition::boundary() const
      {
        BoundaryNode *boundary = this->findBoundary<BoundaryNode>();
        assert(boundary && "NodeComposition::boundary requires BoundaryNode");
        return boundary;
      }

      Scene *NodeComposition::scene() const
      {
        BoundaryNode *owner = boundary();
        Scene *scene = owner ? owner->getScene() : 0;
        assert(scene && "NodeComposition::scene requires Scene");
        return scene;
      }

      ::Window *NodeComposition::window() const
      {
        Scene *scene = this->scene();
        ::Window *window = scene ? scene->getWindow() : 0;
        assert(window && "NodeComposition::window requires Window");
        return window;
      }

    }
  }
}
