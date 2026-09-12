#ifndef LOKA_TESTS_WINDOW_ADMISSION_TEST_APP_HPP
#define LOKA_TESTS_WINDOW_ADMISSION_TEST_APP_HPP

#include "app/core/App.hpp"
#include "app/core/Window.hpp"

/** Exercises the production App clock with fixture-owned windows. The fixture
    keeps them alive through flush(); destruction returns the borrowed rows. */
class WindowAdmissionTestApp : public App
{
public:
  explicit WindowAdmissionTestApp(Window &first, Window *second = 0) : App(0)
  {
    this->group_ = new AppComponentGroup(std::vector<AppComponent *>(1, &first));
    if (second)
      this->group_->adopt(second);
  }
  virtual ~WindowAdmissionTestApp() { this->group_->build(); }
  virtual void quit() {}
  void flush() { this->flushWindowInvalidations(); }
};

#endif
