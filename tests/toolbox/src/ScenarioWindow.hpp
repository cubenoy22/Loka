#ifndef LOKA_TESTS_TOOLBOX_SCENARIO_WINDOW_HPP
#define LOKA_TESTS_TOOLBOX_SCENARIO_WINDOW_HPP

#include "ObservedMainDefinition.hpp"
#include "app/core/WindowDefinition.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    /** Builds the mechanically-shared Toolbox scenario Window around one
        example-owned main Boundary definition. */
    template <class PropsT, class NodeT>
    WindowDefinition<WindowProps> MakeScenarioWindow(const PropsT &props,
                                                     NodeT **observed,
                                                     int width,
                                                     int height,
                                                     const char *title,
                                                     WindowProps::OnIdleFn onIdle,
                                                     void *userData)
    {
      ObservedMainDefinition<PropsT, NodeT> mainDefinition(props, observed);
      return WindowDef(WindowProps()
                           .frame(40, 40, width, height)
                           .scene(mainDefinition)
                           .title(title)
                           .visible(true)
                           .idlePolicy(app::IdlePolicy::everyTick())
                           .onIdle(onIdle, userData));
    }
  } // namespace toolbox_tests
} // namespace loka

#endif // LOKA_TESTS_TOOLBOX_SCENARIO_WINDOW_HPP
