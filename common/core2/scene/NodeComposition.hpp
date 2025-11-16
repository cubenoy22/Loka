#ifndef DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP

#include <vector>
#include "core2/scene/Node.hpp"
#include "core2/scene/StreamView.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      struct NodeComposition
      {
      private:
        // アリーナ: compose中に作られた全てのDefinitionのコピーを所有する
        std::vector<NodeDefinitionBase *> arena_;
        NodeDefinitionBase *root_;

      public:
        NodeComposition() : root_(0) {}

        ~NodeComposition()
        {
          for (size_t i = 0; i < arena_.size(); ++i)
          {
            delete arena_[i];
          }
        }

        // アリーナにDefinitionのコピーを作成し、そのポインタを返す
        template <typename T>
        T *copyToArena(const T &def)
        {
          T *newDef = new T(def); // コピーコンストラクタでヒープにコピー
          arena_.push_back(newDef);
          return newDef;
        }

        // ルートノードを宣言する
        template <typename T>
        T &declare(const T &def)
        {
          T *newRoot = this->copyToArena(def);
          this->root_ = newRoot;
          return *newRoot;
        }

        // Nodeツリーを生成する
        Node *createNodeTree() const;

        NodeDefinitionBase *root() const { return root_; }

        // --- 既存のDSL用メソッド ---
        template <typename T>
        declara::core::scene::ConditionalDefinition conditional(const State<bool> &condition, T &x)
        {
          extern T Empty;
          return ConditionalDefinition(ConditionalProps(&condition, &x, &Empty));
        }

        template <typename T>
        declara::core::scene::ConditionalDefinition conditional(State<bool> &condition, T &x)
        {
          extern T Empty;
          return ConditionalDefinition(ConditionalProps(&condition, &x, &Empty));
        }

        template <typename T>
        StreamView<typename std::vector<T>::const_iterator> stream(const std::vector<T> &v)
        {
          return StreamView<typename std::vector<T>::const_iterator>(v.begin(), v.end());
        }

        template <typename It>
        StreamView<It> stream(It begin, It end)
        {
          return StreamView<It>(begin, end);
        }

        template <typename T>
        T &group(T &x) { return x; }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
