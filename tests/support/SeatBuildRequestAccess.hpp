#ifndef LOKA_TESTS_SEAT_BUILD_REQUEST_ACCESS_HPP
#define LOKA_TESTS_SEAT_BUILD_REQUEST_ACCESS_HPP
#include "app/scene/boundary/Boundary.hpp"
namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class SeatBuildRequestAccess
      {
      public:
        static ::loka::app::scene::Node *returning(const ::loka::app::scene::detail::SeatBuildRequest &request) { return request.outgoing_; }
        static core::StateBase *firstCondition(::loka::app::scene::BoundaryNode &owner)
        { return owner.branchSeats_.plans()[0].seat()->branchCondition(); }
        static void holdRetirement(::loka::app::scene::BoundaryNode &owner, bool hold)
        { owner.drainingRetiredSubtrees_ = hold; }
        static const ::loka::app::scene::detail::SeatReservation *firstReservation(::loka::app::scene::BoundaryNode &owner)
        {
          return owner.branchSeats_.plans()[0].seat()->seatReservation();
        }
        static const ::loka::app::scene::detail::SeatReservation *nestedReservation(::loka::app::scene::BoundaryNode &owner)
        {
          return owner.branchSeats_.plans()[0].seat()->declaredBranchSeats()->plans()[0].seat()->seatReservation();
        }
        static unsigned scopeReferences(const ::loka::app::scene::detail::SeatReservation &seat)
        {
          return (seat.request().source_ != 0 ? 1u : 0u) + (seat.request().position_.parent != 0 ? 1u : 0u);
        }
        static bool remove(::loka::app::scene::BoundaryNode &owner, const ::loka::app::scene::detail::SeatReservation &seat, ::loka::app::scene::Node *node, int order)
        {
          seat.request().mark();
          return owner.seatReservations_.removeSeatChild(seat.request(), &owner, node, order);
        }
        static bool install(::loka::app::scene::BoundaryNode &owner, const ::loka::app::scene::detail::SeatReservation &seat, ::loka::app::scene::Node *node)
        {
          return owner.seatReservations_.installSeatChild(seat.request(), node);
        }
        static void park(::loka::app::scene::BoundaryNode &owner, ::loka::app::scene::Node *node)
        {
          owner.parkBranch(::loka::app::scene::BoundaryParkedBranchKey(9002, 0, 0, 0), node, 0);
        }
      };
    } // namespace testing
  } // namespace dsl
} // namespace loka

#endif
