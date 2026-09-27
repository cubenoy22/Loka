#ifndef LOKA_WIN32_TEXT_ENVIRONMENT_HPP
#define LOKA_WIN32_TEXT_ENVIRONMENT_HPP

#include <cassert>

/** Controller-owned delivery to its text-context subscriptions, including parked
    contexts. Notifications revoke borrowed fonts before their owner swaps. */
class Win32TextEnvironment
{
public:
  /** Borrowed invalidation target; the subscribing text context owns it. */
  class Listener
  {
  public:
    virtual void onTextEnvironmentChanged() = 0;

  protected:
    ~Listener() {}
  };

  /** Allocation-free registry membership owned by one text context. */
  class Subscription
  {
  public:
    Subscription(Win32TextEnvironment &owner, Listener &listener)
        : owner_(&owner),
          next_(owner.head_),
          listener_(listener)
    {
      owner.head_ = this;
    }
    ~Subscription()
    {
      this->disconnectTextEnvironment();
    }
    /** Unlinks over preceding text rows in this controller's registry. */
    void disconnectTextEnvironment()
    {
      if (!this->owner_)
        return;
      Subscription **link = &this->owner_->head_;
      while (*link != this)
        link = &(*link)->next_;
      *link = this->next_;
      this->owner_ = 0;
      this->next_ = 0;
    }

  private:
    friend class Win32TextEnvironment;
    Win32TextEnvironment *owner_;
    Subscription *next_;
    Listener &listener_;
    Subscription(const Subscription &);
    Subscription &operator=(const Subscription &);
  };

  Win32TextEnvironment()
      : head_(0)
  {
  }
  ~Win32TextEnvironment()
  {
    assert(!this->head_);
  }
  /** Walks this controller's text-context rows, including parked text.
      Synchronous invalidation only: subscribers must not mutate this list. */
  void changed()
  {
    for (Subscription *entry = this->head_; entry; entry = entry->next_)
      entry->listener_.onTextEnvironmentChanged();
  }

private:
  Subscription *head_;
  Win32TextEnvironment(const Win32TextEnvironment &);
  Win32TextEnvironment &operator=(const Win32TextEnvironment &);
};
#endif
