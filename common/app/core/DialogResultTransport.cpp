#include "app/core/DialogResultTransport.hpp"
#include <new>
#include <cassert>

namespace loka
{
  namespace app
  {
    DialogResultTransport::Entry::Entry(DialogResultTransport &owner)
        : transport(owner),
          registration(0),
          returnPort(0),
          result(),
          chain(0),
          next(0),
          previous(0)
    {
    }

    void DialogResultTransport::Entry::unlink()
    {
      if (!this->previous)
        return;
      *this->previous = this->next;
      if (this->next)
        this->next->previous = this->previous;
      this->chain = 0;
      this->next = 0;
      this->previous = 0;
    }

    void DialogResultTransport::Entry::moveTo(Chain &destination)
    {
      this->unlink();
      this->chain = &destination;
      this->next = destination.head;
      this->previous = &destination.head;
      if (this->next)
        this->next->previous = &this->next;
      destination.head = this;
    }

    DialogResultTransport::Registration::Registration(const OpenFileDialogProps &props)
        : binding_(props),
          entry_(0)
    {
    }

    DialogResultTransport::Registration::~Registration()
    {
      if (this->entry_)
        this->entry_->transport.cancel(*this->entry_);
    }

    bool DialogResultTransport::Registration::matches(const OpenFileDialogProps &props) const
    {
      return this->binding_.result_.state() == props.result_.state() && this->binding_.onResult_ == props.onResult_
             && this->binding_.windowToAttach_ == props.windowToAttach_;
    }

    DialogResultTransport::ReturnPort::ReturnPort(Registration *registration)
        : entry_(0)
    {
      if (registration && registration->entry_
          && registration->entry_->chain == &registration->entry_->transport.reserved_
          && !registration->entry_->returnPort)
      {
        this->entry_ = registration->entry_;
        this->entry_->returnPort = this;
      }
    }

    DialogResultTransport::ReturnPort::~ReturnPort()
    {
      if (this->entry_)
        this->entry_->transport.cancel(*this->entry_);
    }

    Window *DialogResultTransport::ReturnPort::seal(const FileChooserResult &result)
    {
      Entry *entry = this->entry_;
      if (!entry)
        return 0;
      this->entry_ = 0;
      entry->returnPort = 0;
      entry->result = result;
      entry->moveTo(entry->transport.pending_);
      return entry->transport.window_;
    }

    DialogResultTransport::DialogResultTransport()
        : window_(0)
    {
    }
    DialogResultTransport::~DialogResultTransport()
    {
      this->close();
      assert(!this->active_.head && "Window destruction must follow App admission return");
      this->reclaim(this->retired_.head);
    }

    void DialogResultTransport::open(Window &window)
    {
      assert(!this->window_ || this->window_ == &window);
      this->window_ = &window;
    }

    DialogResultTransport::Registration *DialogResultTransport::reserve(const OpenFileDialogProps &props)
    {
      // Internal binding validation reads the handle's owner route, never the
      // transaction-local StateBase::trackerOwner(). It does not create State.
      if (!this->window_ || (props.result_.isValid() && !props.result_.dangerouslyTracker()))
        return 0;
      Registration *registration = new (std::nothrow) Registration(props);
      if (!registration)
        return 0;
      Entry *entry = new (std::nothrow) Entry(*this);
      if (!entry)
      {
        delete registration;
        return 0;
      }
      registration->entry_ = entry;
      entry->registration = registration;
      entry->moveTo(this->reserved_);
      return registration;
    }

    void DialogResultTransport::revoke(Entry &entry)
    {
      if (entry.registration)
      {
        entry.registration->entry_ = 0;
        entry.registration->binding_ = OpenFileDialogProps();
        entry.registration = 0;
      }
      if (entry.returnPort)
      {
        entry.returnPort->entry_ = 0;
        entry.returnPort = 0;
      }
    }

    void DialogResultTransport::cancel(Entry &entry)
    {
      revoke(entry);
      if (entry.chain != &this->active_)
        entry.moveTo(this->retired_);
    }

    void DialogResultTransport::close()
    {
      this->window_ = 0;
      while (this->reserved_.head)
        this->cancel(*this->reserved_.head);
      while (this->pending_.head)
        this->cancel(*this->pending_.head);
      for (Entry *entry = this->active_.head; entry; entry = entry->next)
        revoke(*entry);
    }

    bool DialogResultTransport::hasRunnableWork() const
    {
      return this->pending_.head || this->retired_.head;
    }

    void DialogResultTransport::deliver()
    {
      assert(!this->active_.head);
      // Capture a finite batch. Reversing the pending stack preserves seal order.
      while (this->pending_.head)
        this->pending_.head->moveTo(this->active_);
      while (this->active_.head)
      {
        Invocation invocation(*this->active_.head);
        invocation.deliver();
      }
    }

    DialogResultTransport::Invocation::Invocation(Entry &entry)
        : entry_(entry),
          emitterToken_(0)
    {
      if (entry.registration && entry.registration->binding_.onResult_)
        this->emitterToken_ = entry.registration->binding_.onResult_->retainExternalLifetimeToken();
    }

    DialogResultTransport::Invocation::~Invocation()
    {
      if (this->emitterToken_)
        loka::core::StateBase::releaseExternalLifetimeToken(this->emitterToken_);
      revoke(this->entry_);
      this->entry_.moveTo(this->entry_.transport.retired_);
    }

    void DialogResultTransport::Invocation::deliver()
    {
      if (!this->entry_.registration)
        return;
      scene::NodeState<FileChooserResult> result = this->entry_.registration->binding_.result_;
      loka::core::EmitterState *emitter = this->entry_.registration->binding_.onResult_;
      if (result.isValid())
        result.set(this->entry_.result, true);
      // set() may synchronously delete the registration or close this Window.
      if (this->entry_.registration && emitter
          && loka::core::StateBase::isExternalLifetimeTokenAlive(this->emitterToken_))
        emitter->emit();
    }

    DialogResultTransport::Entry *DialogResultTransport::retirementSnapshot() const
    {
      return this->retired_.head;
    }

    void DialogResultTransport::reclaim(Entry *snapshot)
    {
      while (snapshot)
      {
        Entry *next = snapshot->next;
        snapshot->unlink();
        delete snapshot;
        snapshot = next;
      }
    }
  } // namespace app
} // namespace loka
