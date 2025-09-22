#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/Node.hpp"

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
          return nullptr;
        }

        // 1. Nodeインスタンスを生成
        Node *node = def->create();

        // 2. 子を持つことができるかチェック
        const INestableDefinition *nestableDef = dynamic_cast<const INestableDefinition *>(def);
        INestable *nestableNode = dynamic_cast<INestable *>(node);

        if (nestableDef && nestableNode)
        {
          // 3. 子Definitionを再帰的にNodeに変換し、親Nodeに追加
          //    (INestableDefinitionが子のリストを返すメソッドを持つ必要がある)
          // const std::vector<NodeDefinitionBase*>& children = nestableDef->getChildren();
          // for (size_t i = 0; i < children.size(); ++i) {
          //   Node* childNode = createNodeRecursive(children[i]);
          //   if (childNode) {
          //     nestableNode->addChild(childNode);
          //   }
          // }
        }

        return node;
      }

      Node *NodeComposition::createNodeTree() const
      {
        return createNodeRecursive(this->root);
      }

    }
  }
}
