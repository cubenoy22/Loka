#ifndef LOKA_TESTS_PLATFORM_NULL_APP_HPP
#define LOKA_TESTS_PLATFORM_NULL_APP_HPP

#include "app/core/App.hpp"
#include "platform/null/NullWindow.hpp"

class NullApp : public App
{
public:
  explicit NullApp(AppConfigurable *config)
      : App(config),
        quitRequested_(false)
  {
  }

  virtual ~NullApp() {}

  virtual void run()
  {
    App::run();
    this->reconcileFocus();
  }

  virtual void quit()
  {
    this->quitRequested_ = true;
  }

  bool quitRequested() const
  {
    return this->quitRequested_;
  }

  const std::vector<Window *> &adoptedWindows() const { return this->adopted_; }
  const std::vector<Window *> &bootstrapRefusedWindows() const { return this->bootstrapRefused_; }

protected:
  virtual bool windowAdopted(Window *window)
  {
    this->adopted_.push_back(window);
    // every NullApp window is a NullWindow (NullPlatformContext::createWindow is the only producer)
    static_cast<NullWindow *>(window)->setApp(this);
    return true;
  }
  virtual void bootstrapWindowRefused(Window *window) { this->bootstrapRefused_.push_back(window); }

private:
  bool quitRequested_;
  std::vector<Window *> adopted_;
  std::vector<Window *> bootstrapRefused_;
};

#endif // LOKA_TESTS_PLATFORM_NULL_APP_HPP
