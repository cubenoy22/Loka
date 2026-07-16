#ifndef LOKA_TESTS_PLATFORM_NULL_APP_HPP
#define LOKA_TESTS_PLATFORM_NULL_APP_HPP

#include "app/core/App.hpp"

class NullApp : public App
{
public:
  explicit NullApp(AppConfigurable *config)
      : App(config),
        quitRequested_(false)
  {
  }

  virtual ~NullApp() {}

  virtual void quit()
  {
    this->quitRequested_ = true;
  }

  bool quitRequested() const
  {
    return this->quitRequested_;
  }

private:
  bool quitRequested_;
};

#endif // LOKA_TESTS_PLATFORM_NULL_APP_HPP
