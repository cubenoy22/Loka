#ifndef LOKA_TESTS_SUPPORT_MEASUREMENT_RETRY_QUEUE_HPP
#define LOKA_TESTS_SUPPORT_MEASUREMENT_RETRY_QUEUE_HPP

#include "app/scene/projection/PlatformController.hpp"
#include "core/scheduler/NextTickTracker.hpp"

/** Host controller stand-in for the native event queue. Each flush executes
    only requested layout work; a refusal during that work awaits another flush. */
class MeasurementRetryQueue
{
public:
  void request() { this->tracker_.requestAfterRun(); }
  bool pending() const { return this->tracker_.hasPendingRequest(); }
  void flush(loka::app::scene::IPlatformController &controller,
             loka::app::scene::Node &node, loka::app::scene::LayoutState &state)
  {
    Attempt attempt(controller, node, state);
    this->tracker_.run(&Attempt::layout, &Attempt::apply, &attempt);
  }
private:
  struct Attempt
  {
    Attempt(loka::app::scene::IPlatformController &c, loka::app::scene::Node &n,
            loka::app::scene::LayoutState &s) : controller(c), node(n), state(s) {}
    static bool layout(void *raw)
    {
      Attempt &attempt = *static_cast<Attempt *>(raw);
      attempt.node.layout(&attempt.controller, attempt.state);
      return false;
    }
    static void apply(void *) {}
    loka::app::scene::IPlatformController &controller;
    loka::app::scene::Node &node;
    loka::app::scene::LayoutState &state;
  };
  loka::core::NextTickTracker tracker_;
};
#endif
