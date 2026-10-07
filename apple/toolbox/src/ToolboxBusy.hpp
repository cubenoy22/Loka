#ifndef LOKA_TOOLBOX_BUSY_HPP
#define LOKA_TOOLBOX_BUSY_HPP

/** The app-wide owner of "the main thread is blocked" feedback, the watch
    cursor of #808. CursorOwner is the one implementation; BusyScope is the
    only caller. */
class ToolboxBusyOwner
{
protected:
  ~ToolboxBusyOwner() {}

private:
  friend class BusyScope;
  virtual void enterBusy() = 0;
  virtual void exitBusy() = 0;
};

/** Synchronous, non-copyable borrow; nested scopes issue no native writes. */
class BusyScope
{
public:
  explicit BusyScope(ToolboxBusyOwner &owner) : owner_(&owner) { this->owner_->enterBusy(); }
  /** A null owner makes a conditional borrow inert, without allocation. */
  explicit BusyScope(ToolboxBusyOwner *owner) : owner_(owner)
  {
    if (this->owner_)
      this->owner_->enterBusy();
  }
  ~BusyScope()
  {
    if (this->owner_)
      this->owner_->exitBusy();
  }

private:
  BusyScope(const BusyScope &);
  BusyScope &operator=(const BusyScope &);
  ToolboxBusyOwner *owner_;
};

/** Registers the process's busy owner for the registration's lifetime, one at
    a time. ToolboxApp holds one around its run loop, so blocking Toolbox work
    with no path to the app (a file read, an image decode) can borrow it
    (#1066). On Classic one process runs one app and owns one cursor. */
class ToolboxBusyOwnerRegistration
{
public:
  explicit ToolboxBusyOwnerRegistration(ToolboxBusyOwner &owner);
  ~ToolboxBusyOwnerRegistration();

private:
  ToolboxBusyOwnerRegistration(const ToolboxBusyOwnerRegistration &);
  ToolboxBusyOwnerRegistration &operator=(const ToolboxBusyOwnerRegistration &);
};

/** The registered owner, or null outside a registration (the borrow is inert). */
ToolboxBusyOwner *RegisteredToolboxBusyOwner();

#endif // LOKA_TOOLBOX_BUSY_HPP
