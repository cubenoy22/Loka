#ifndef LOKA_APP_FOCUS_PUBLISHER_HPP
#define LOKA_APP_FOCUS_PUBLISHER_HPP

#include "app/FocusParticipant.hpp"
class Window;

namespace loka
{
  namespace app
  {
    namespace detail
    {
      /** Stateless publication policy, entered through Window completion.
          Debug audits run only on publication and published binding exchange:
          they detect attached duplicates in that Scene and facts simultaneously
          published by another live Scene, not every borrowed or registered fact. */
      class FocusPublisher
      {
        static void reconcile(scene::SceneFocus &current, bool answered, scene::NodeContext *target);
        static void leave(FocusParticipant &row);
        static void rebind(FocusParticipant &row, const FocusBinding &previous);
        static void exchange(scene::SceneFocus &current, FocusParticipant *target, const FocusBinding &previous);
        static void audit(scene::SceneFocus &current, const FocusParticipant &target);
        static bool attached(const FocusParticipant &row);
        friend class ::Window;
        friend class ::loka::app::FocusParticipant;
      };
    } // namespace detail
  } // namespace app
} // namespace loka
#endif
