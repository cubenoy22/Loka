#ifndef LOKA_TESTS_PLATFORM_NULL_WINDOW_HPP
#define LOKA_TESTS_PLATFORM_NULL_WINDOW_HPP

#include "app/core/Window.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"

class NullWindow : public Window
{
public:
  NullWindow(PlatformContext *context,
             const WindowProps &props,
             NullScenePlatformController *borrowedController = 0)
      : Window(context, props),
        controller_(borrowedController),
        ownsController_(borrowedController == 0),
        mountedScene_(false)
  {
    if (!this->controller_)
    {
      this->controller_ = new NullScenePlatformController();
    }
    this->mountScene();
  }

  virtual ~NullWindow()
  {
    this->destroyScenePlatform();
  }

  /** Native teardown body used by App admission and terminal destruction. */
  void destroyScenePlatform()
  {
    this->teardownScene();
    if (this->controller_)
    {
      this->controller_->destroy();
      this->controller_->recordWindowDisposed();
    }
    if (this->ownsController_)
    {
      delete this->controller_;
    }
    this->controller_ = 0;
  }

  void mountScene()
  {
    loka::app::scene::Scene *currentScene = this->scene();
    if (!currentScene || this->mountedScene_ || !this->controller_)
    {
      return;
    }
    this->mountReplacementScene(currentScene);
    this->mountedScene_ = true;
  }

  /** Mirrors the concrete windows' teardown ordering: the scene unmounts
      (and severs its controller pointer) before the controller can be
      deleted, so the scene manager's later scene destruction never reaches
      a freed controller. */
  void teardownScene()
  {
    if (!this->mountedScene_)
    {
      return;
    }
    loka::app::scene::Scene *currentScene = this->scene();
    if (currentScene)
    {
      this->unmountSceneForTeardown(*currentScene);
    }
    this->mountedScene_ = false;
  }

  NullScenePlatformController *scenePlatformController() const
  {
    return this->controller_;
  }

  virtual bool hasPendingScenePlatformSync() const
  {
    return this->controller_ ? this->controller_->hasPendingSync() : false;
  }

  virtual void synchronizeScenePlatform()
  {
    if (this->controller_)
    {
      this->controller_->synchronize();
    }
  }

  virtual void drainNativeRetirements()
  {
    if (this->controller_)
    {
      this->controller_->drainNativeRetirements();
    }
  }

protected:
  virtual bool hasLiveScenePlatform() const
  {
    return this->controller_ && this->mountedScene_;
  }

  virtual bool mountReplacementScene(loka::app::scene::Scene *next)
  {
    if (this->controller_)
    {
      next->mount(this->controller_);
      this->mountedScene_ = true;
    }
    return true;
  }

private:
  // Deliberate counterpart of Win32's HWND comparison. This rail uses its
  // controller identity as fake native presence, independently of scene mount.
  virtual bool hasPendingNativeVisibility() const
  {
    return this->visibility_->get() != (this->controller_ != 0);
  }

  virtual void applyNativeVisibility()
  {
    if (!this->hasPendingNativeVisibility())
      return;
    if (this->visibility_->get())
    {
      this->controller_ = new NullScenePlatformController();
      this->ownsController_ = true;
      this->mountScene();
    }
    else
      this->destroyScenePlatform();
  }

  NullScenePlatformController *controller_;
  bool ownsController_;
  bool mountedScene_;

  NullWindow(const NullWindow &);
  NullWindow &operator=(const NullWindow &);
};

#endif // LOKA_TESTS_PLATFORM_NULL_WINDOW_HPP
