#ifndef DECLARA_PLATFORMCONTEXT_HPP
#define DECLARA_PLATFORMCONTEXT_HPP

class PlatformContext;
class Scene;

class PlatformContext
{
public:
  virtual ~PlatformContext() {}

  // Scene ライフサイクルイベント
  virtual void onSceneCreate(class Scene *scene) = 0;
  virtual void onSceneAttach(class Scene *scene) = 0;
  virtual void onSceneDetach(class Scene *scene) = 0;
  virtual void onSceneDestroy(class Scene *scene) = 0;
};

#endif // DECLARA_PLATFORMCONTEXT_HPP
