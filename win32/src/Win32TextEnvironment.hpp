#ifndef LOKA_WIN32_TEXT_ENVIRONMENT_HPP
#define LOKA_WIN32_TEXT_ENVIRONMENT_HPP

#include <cassert>

/** Controller-owned delivery to its context subscriptions, including parked
    contexts. Notifications revoke borrowed fonts before their owner swaps. */
class Win32TextEnvironment
{
public:
  class Subscription
  {
  public:
    explicit Subscription(Win32TextEnvironment &owner)
        : owner_(&owner),
          next_(owner.head_)
    {
      owner.head_ = this;
    }
    virtual ~Subscription()
    {
      this->disconnectTextEnvironment();
    }
    virtual void onTextEnvironmentChanged() {}

  protected:
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
  /** Synchronous invalidation only: subscribers must not mutate this list. */
  void changed()
  {
    for (Subscription *entry = this->head_; entry; entry = entry->next_)
      entry->onTextEnvironmentChanged();
  }

private:
  Subscription *head_;
  Win32TextEnvironment(const Win32TextEnvironment &);
  Win32TextEnvironment &operator=(const Win32TextEnvironment &);
};
#endif
