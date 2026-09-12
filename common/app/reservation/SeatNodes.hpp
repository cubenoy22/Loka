#ifndef LOKA_APP_RESERVATION_SEAT_NODES_HPP
#define LOKA_APP_RESERVATION_SEAT_NODES_HPP

#include "app/scene/boundary/detail/SeatReservation.hpp"

namespace loka
{
  namespace app
  {
    namespace reservation
    {

      /** Explicit end of a closed runtime-node payload list. */
      struct End
      {
      };
      /** Maximum count of a completed runtime node type, followed by another list. */
      template <class T, int N, class Tail = End> struct Nodes
      {
      };
      /** Required Keyed payload envelope. Framework roots are added internally. */
      template <class List> struct SeatNodes
      {
      };

      namespace detail
      {
        template <class T> struct False
        {
          enum
          {
            value = 0
          };
        };
        template <class T> struct NodeConvertible
        {
          static char accept(scene::Node *);
          template <class U> static char test(char (*)[sizeof(accept(static_cast<U *>(0)))]);
          template <class U> static long test(...);
          enum
          {
            value = sizeof(test<T>(0)) == sizeof(char)
          };
        };
        template <class List> struct ListCheck
        {
          typedef char seat_node_list_must_end_in_End[False<List>::value ? 1 : -1];
          enum
          {
            leaves = 0
          };
        };
        template <> struct ListCheck<End>
        {
          enum
          {
            leaves = 0
          };
        };
        template <class T, int N, class Tail> struct ListCheck<Nodes<T, N, Tail> >
        {
          typedef char seat_node_type_must_be_complete[(sizeof(T) > 0) ? 1 : -1];
          typedef char seat_node_type_must_convert_to_Node[NodeConvertible<T>::value ? 1 : -1];
          typedef char seat_node_count_must_be_positive[(N > 0) ? 1 : -1];
          enum
          {
            leaves = 1 + ListCheck<Tail>::leaves
          };
        };
        /** Only library list specializations emit; applications cannot supply callbacks. */
        template <class List> struct Emitter;
        template <> struct Emitter<End>
        {
          static bool emit(scene::detail::SeatLayoutTable &)
          {
            return true;
          }
        };
        template <class T, int N, class Tail> struct Emitter<Nodes<T, N, Tail> >
        {
          static bool emit(scene::detail::SeatLayoutTable &table)
          {
            return table.append(scene::detail::NodeSlotLayout::of<T>(N)) && Emitter<Tail>::emit(table);
          }
        };
        template <class List> inline void validate()
        {
          typedef char seat_descriptor_exceeds_installation_workspace
              [(int(ListCheck<List>::leaves) <= int(scene::detail::SeatLayoutTable::capacity)) ? 1 : -1];
          (void)sizeof(ListCheck<List>);
          (void)sizeof(seat_descriptor_exceeds_installation_workspace);
        }
      } // namespace detail
    } // namespace reservation
  } // namespace app
} // namespace loka
#endif
