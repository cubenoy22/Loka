#ifndef LOKA_TOOLBOX_COMPOSITION_REPLAY_HPP
#define LOKA_TOOLBOX_COMPOSITION_REPLAY_HPP

#include <cassert>

/** Owner-side membership of projections requiring composition-order replay.
    Unlike transient paint answers, membership lasts from attach to retirement.
    The controller outlives registrations; querying demand never visits nodes. */
class ToolboxCompositionReplay
{
public:
  /** Context-owned, allocation-free membership. Unlinks synchronously in O(1),
      using the pointer-to-pointer shape of DialogResultTransport::Entry. */
  class Registration
  {
  public:
    Registration()
        : previous_(0),
          next_(0)
    {
    }
    ~Registration()
    {
      this->clear();
    }
    void attach(ToolboxCompositionReplay &owner)
    {
      if (this->previous_)
        return;
      this->next_ = owner.first_;
      this->previous_ = &owner.first_;
      if (this->next_)
        this->next_->previous_ = &this->next_;
      owner.first_ = this;
    }
    void clear()
    {
      if (!this->previous_)
        return;
      *this->previous_ = this->next_;
      if (this->next_)
        this->next_->previous_ = this->previous_;
      this->previous_ = 0;
      this->next_ = 0;
    }

  private:
    Registration **previous_;
    Registration *next_;
    Registration(const Registration &);
    Registration &operator=(const Registration &);
  };

  ToolboxCompositionReplay()
      : first_(0)
  {
  }
  ~ToolboxCompositionReplay()
  {
    assert(!this->first_);
  }
  bool required() const
  {
    return this->first_ != 0;
  }

private:
  friend class Registration;
  Registration *first_;
  ToolboxCompositionReplay(const ToolboxCompositionReplay &);
  ToolboxCompositionReplay &operator=(const ToolboxCompositionReplay &);
};
#endif
