#include "app/NativeResourceRetirement.hpp"
#include "app/internal/NativeResourceReservation.hpp"
#include "app/PlatformContext.hpp"
#include "core/LokaAlloc.hpp"
#include "core/Operation.hpp"
#include "core/resource/Image.hpp"
#include <cassert>

namespace loka
{
  namespace app
  {
    namespace
    {
      core::LokaAllocationSite ticketSite()
      {
        return core::LokaAllocationSite("NativeResourceRetirement", "Ticket");
      }
    } // namespace

    NativeResourceRetirement::Ticket::Ticket()
        : owner(0),
          prev(0),
          next(0),
          handle(0),
          dispose(0)
    {
    }

    NativeResourceRetirement::NativeResourceRetirement()
        : held_(0),
          queued_(0),
          inFlight_(0)
    {
    }

    void NativeResourceRetirement::Ticket::unlinkHeld()
    {
      if (this->prev)
        this->prev->next = this->next;
      else
        this->owner->held_ = this->next;
      if (this->next)
        this->next->prev = this->prev;
      this->prev = 0;
      this->next = 0;
    }

    void NativeResourceRetirement::Ticket::enqueue(void *nativeHandle)
    {
      if (!this->owner)
      {
        core::LokaDelete(this, ticketSite());
        return;
      }
      this->unlinkHeld();
      this->handle = nativeHandle;
      this->next = this->owner->queued_;
      this->owner->queued_ = this;
    }

    void NativeResourceRetirement::ReleaseThroughTicket(void *handle, void *userData)
    {
      static_cast<Ticket *>(userData)->enqueue(handle);
    }

    void NativeResourceRetirement::drain()
    {
      if (core::Operation::hasActive() || this->inFlight_)
        return;
      this->inFlight_ = this->queued_;
      this->queued_ = 0;
      while (this->inFlight_)
      {
        Ticket *current = this->inFlight_;
        current->dispose(current->handle);
        this->inFlight_ = current->next;
        core::LokaDelete(current, ticketSite());
      }
    }

    NativeResourceRetirement::~NativeResourceRetirement()
    {
#ifdef LOKA_LIFECYCLE_AUDIT
      assert(!core::Operation::hasActive());
      assert(!this->inFlight_);
#endif
      std::size_t heldCount = 0;
      for (Ticket *ticket = this->held_; ticket; ticket = ticket->next)
      {
#ifdef LOKA_LIFECYCLE_AUDIT
        assert(ticket->handle && "PlatformContext destroyed with an armed Reservation");
#endif
        ++heldCount;
      }
      // One initial snapshot, then at most one per held ticket at entry.
      do
      {
        this->drain();
      } while (this->queued_ && heldCount-- != 0);
#ifdef LOKA_LIFECYCLE_AUDIT
      assert(!this->held_ && !this->queued_ && !this->inFlight_);
#endif
      // Invalid late Image lifetime: revoke its back edge without native work.
      while (this->held_)
      {
        Ticket *ticket = this->held_;
        ticket->unlinkHeld();
        ticket->owner = 0;
      }
    }

    namespace internal
    {
      Reservation::Reservation(PlatformContext &context, void (*dispose)(void *))
          : ticket_(0)
      {
        assert(dispose);
        NativeResourceRetirement &owner = context.nativeResourceRetirement_;
#ifdef LOKA_LIFECYCLE_AUDIT
        assert(!owner.inFlight_ && "Reservation inside native disposal");
#endif
        this->ticket_ = core::LokaNew<NativeResourceRetirement::Ticket>(ticketSite());
        if (!this->ticket_)
          return;
        this->ticket_->owner = &owner;
        this->ticket_->dispose = dispose;
        this->ticket_->next = owner.held_;
        if (owner.held_)
          owner.held_->prev = this->ticket_;
        owner.held_ = this->ticket_;
      }

      Reservation::~Reservation()
      {
        if (!this->ticket_)
          return;
        this->ticket_->unlinkHeld();
        core::LokaDelete(this->ticket_, ticketSite());
      }

      bool Reservation::publishImage(void *handle, int width, int height, core::resource::Image &out)
      {
        if (!this->ticket_ || !handle)
          return false;
        NativeResourceRetirement::Ticket *ticket = this->ticket_;
        this->ticket_ = 0;
        ticket->handle = handle;
        out = core::resource::Image::FromNative(
            handle, width, height, &NativeResourceRetirement::ReleaseThroughTicket, ticket);
        return out.isValid();
      }
    } // namespace internal
  } // namespace app
} // namespace loka
