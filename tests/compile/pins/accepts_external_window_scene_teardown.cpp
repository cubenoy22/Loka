#include "app/core/Window.hpp"

namespace external
{
  /** An external backend needs no Scene friendship to tear down its Window. */
  class CustomWindow : public Window
  {
  public:
    explicit CustomWindow(PlatformContext *context)
        : Window(context)
    {
    }

    virtual ~CustomWindow()
    {
      this->teardownScene();
    }

  private:
    void teardownScene()
    {
      loka::app::scene::Scene *currentScene = this->scene();
      if (currentScene)
      {
        this->unmountSceneForTeardown(*currentScene);
      }
    }
  };
}
