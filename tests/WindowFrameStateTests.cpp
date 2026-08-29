#include "WindowFrameStateTests.hpp"

#include <cassert>
#include "app/PlatformContext.hpp"
#include "app/core/Window.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullPlatformContext.hpp"

namespace
{
  static void incrementWindowFrameNotification(void *userData)
  {
    ++(*static_cast<int *>(userData));
  }

  struct WindowWidthIsNarrow : public loka::core::DerivedState<bool>::EvalFn
  {
    loka::core::State<loka::core::Frame> *frame;

    explicit WindowWidthIsNarrow(loka::core::State<loka::core::Frame> *frameState)
        : frame(frameState)
    {
    }

    virtual bool operator()()
    {
      return this->frame && this->frame->get().width < 480;
    }
  };
} // namespace

void testWindowFrameStateDrivesDerivedState()
{
  NullPlatformContext context;
  loka::core::MutableState<loka::core::Frame> frame(loka::core::Frame(10, 20, 640, 480));
  WindowProps props;
  props.frameState(&frame);
  Window *window = context.createWindow(props);
  assert(window);

  loka::core::DerivedState<bool> narrow(&frame, new WindowWidthIsNarrow(&frame));
  loka::core::PushStateTracker *tracker = window->getTracker()->asPushTracker();
  assert(tracker);
  tracker->addState(&narrow);
  int notifications = 0;
  narrow.bind(&incrementWindowFrameNotification, &notifications, false);

  {
    loka::core::StateTrackerGuard guard(window->getTracker());
    frame.set(loka::core::Frame(10, 20, 400, 480));
  }
  assert(narrow.get());
  assert(notifications == 1);

  {
    loka::core::StateTrackerGuard guard(window->getTracker());
    frame.set(loka::core::Frame(10, 20, 300, 480));
  }
  assert(narrow.get());
  assert(notifications == 1);

  {
    loka::core::StateTrackerGuard guard(window->getTracker());
    frame.set(loka::core::Frame(10, 20, 640, 480));
  }
  assert(!narrow.get());
  assert(notifications == 2);

  tracker->removeState(&narrow);
  delete window;
}
