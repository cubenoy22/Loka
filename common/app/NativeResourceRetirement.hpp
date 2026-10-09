#ifndef LOKA_APP_NATIVE_RESOURCE_RETIREMENT_HPP
#define LOKA_APP_NATIVE_RESOURCE_RETIREMENT_HPP

class App;
class PlatformContext;

namespace loka
{
  namespace app
  {
    namespace internal
    {
      class Reservation;
    }
    namespace testing
    {
      class NativeResourceRetirementTestAccess;
    }

    /** PlatformContext-owned tickets defer native side effects to its clock.
        Producers enter through the rail-internal Reservation header. */
    class NativeResourceRetirement
    {
    public:
      NativeResourceRetirement();
      ~NativeResourceRetirement();

    private:
      friend class ::App;
      friend class internal::Reservation;
      friend class testing::NativeResourceRetirementTestAccess;

      /** Intrusive lifetime obligation; null handle means unpublished reservation. */
      class Ticket
      {
      public:
        Ticket();

      private:
        friend class NativeResourceRetirement;
        friend class internal::Reservation;
        friend class testing::NativeResourceRetirementTestAccess;
        void unlinkHeld();
        void enqueue(void *nativeHandle);
        NativeResourceRetirement *owner;
        Ticket *prev;
        Ticket *next;
        void *handle;
        void (*dispose)(void *);
      };

      static void ReleaseThroughTicket(void *handle, void *userData);
      void drain();
      Ticket *held_;
      Ticket *queued_;
      Ticket *inFlight_;

      NativeResourceRetirement(const NativeResourceRetirement &);
      NativeResourceRetirement &operator=(const NativeResourceRetirement &);
    };
  } // namespace app
} // namespace loka
#endif
