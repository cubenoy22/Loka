#ifndef LOKA_APP_INTERNAL_NATIVE_RESOURCE_RESERVATION_HPP
#define LOKA_APP_INTERNAL_NATIVE_RESOURCE_RESERVATION_HPP

#include "app/NativeResourceRetirement.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      class Image;
    }
  } // namespace core
  namespace app
  {
    namespace internal
    {
      /** Rail-only stack reservation. Reserve before acquiring any native resource.
          The disposer must be rail static code, never an application callback.
          Roll back native construction inline until publishImage consumes it. */
      class Reservation
      {
      public:
        Reservation(PlatformContext &context, void (*dispose)(void *));
        ~Reservation();
        bool isValid() const
        {
          return this->ticket_ != 0;
        }
        /** A non-null handle consumes the reservation even if Image allocation
            refuses. A null handle leaves cancellation to the destructor. */
        bool publishImage(void *handle, int width, int height, core::resource::Image &out);

      private:
        NativeResourceRetirement::Ticket *ticket_;
        Reservation(const Reservation &);
        Reservation &operator=(const Reservation &);
      };
    } // namespace internal
  } // namespace app
} // namespace loka
#endif
