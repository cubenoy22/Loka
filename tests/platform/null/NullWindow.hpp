#ifndef LOKA_TESTS_PLATFORM_NULL_WINDOW_HPP
#define LOKA_TESTS_PLATFORM_NULL_WINDOW_HPP

#include "app/core/Window.hpp"
#include "platform/null/NullScenePlatformController.hpp"

class NullWindow : public Window
{
public:
  NullWindow(PlatformContext *context,
             const WindowProps &props,
             NullScenePlatformController *borrowedController = 0)
      : Window(context, props),
        controller_(borrowedController),
        ownsController_(borrowedController == 0)
  {
    if (!this->controller_)
    {
      this->controller_ = new NullScenePlatformController();
    }
  }

  virtual ~NullWindow()
  {
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

  NullScenePlatformController *scenePlatformController() const
  {
    return this->controller_;
  }

private:
  NullScenePlatformController *controller_;
  bool ownsController_;

  NullWindow(const NullWindow &);
  NullWindow &operator=(const NullWindow &);
};

#endif // LOKA_TESTS_PLATFORM_NULL_WINDOW_HPP
