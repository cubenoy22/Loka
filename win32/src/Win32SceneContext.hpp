#ifndef DECLARA_WIN32_SCENECONTEXT_HPP
#define DECLARA_WIN32_SCENECONTEXT_HPP

#include "core/SceneContext.hpp"

// Win32固有のSceneContext実装
class Win32SceneContext : public SceneContext
{
public:
  Win32SceneContext(Scene *scene); // Scene* を受け取るコンストラクタに変更
  virtual ~Win32SceneContext();
  // 必要に応じてWin32固有のリソース管理APIを追加
  // --- SceneNodeContext生成・再取得API ---
  virtual SceneNodeContext *getNodeContext(SceneNode *node) { return 0; }

private:
  Scene *scene_; // Scene への非所有ポインタ参照
};

#endif // DECLARA_WIN32_SCENECONTEXT_HPP
