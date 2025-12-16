#ifndef DECLARA_CORE2_SCENE_NODEMANAGER_HPP
#define DECLARA_CORE2_SCENE_NODEMANAGER_HPP

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class Scene;
      class Node;
      class IPlatformController;
      class ComposableNode;

      // 抽象ノード管理インターフェース（今後のReact型等にも差し替え可能）
      class INodeManager
      {
      public:
        virtual ~INodeManager() {}
        // SceneとPlatformControllerを関連付け、ノードツリーを構築
        virtual bool mount(Scene *scene, IPlatformController *platformController) = 0;
        // ノードツリーを破棄し、PlatformControllerへdestroy通知
        virtual void unmount() = 0;
        // 現在保持しているノードツリーのルート
        virtual Node *root() const = 0;
      };

      // Solid.js型の静的なノードツリーを扱う実装
      class StaticNodeManager : public INodeManager
      {
      public:
        StaticNodeManager();
        virtual ~StaticNodeManager();

        virtual bool mount(Scene *scene, IPlatformController *platformController);
        virtual void unmount();
        virtual Node *root() const;

      private:
        void clearNodeTree();

        Scene *scene_;
        Node *rootNode_;
        IPlatformController *platformController_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODEMANAGER_HPP
