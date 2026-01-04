#ifndef LOKA_CORE_MANAGED_HPP
#define LOKA_CORE_MANAGED_HPP

#include <cstddef>

// Managed<T>: Simple intrusive reference-counted handle for platform resources.
// - Wraps a pointer to T plus an optional releaser callback.
// - Copying increments the refcount, destruction decrements it.
// - When the count reaches zero, the releaser is invoked and the control block is destroyed.
template <typename T>
class Managed
{
public:
  typedef void (*ReleaserFn)(T *value, void *userData);

  Managed() : block_(0) {}
  Managed(const Managed &other) : block_(other.block_)
  {
    retain();
  }
  Managed &operator=(const Managed &other)
  {
    if (this != &other)
    {
      release();
      block_ = other.block_;
      retain();
    }
    return *this;
  }
  ~Managed()
  {
    release();
  }

  static Managed<T> Wrap(T *value, ReleaserFn releaser = &Managed::DefaultDelete, void *userData = 0)
  {
    if (!value)
      return Managed<T>();
    return Managed<T>(new ControlBlock(value, releaser, userData));
  }

  T *get() const { return block_ ? block_->value : 0; }
  T &operator*() const { return *block_->value; }
  T *operator->() const { return block_ ? block_->value : 0; }
  bool isValid() const { return block_ && block_->value; }
  int useCount() const { return block_ ? block_->refCount : 0; }

  void reset()
  {
    release();
    block_ = 0;
  }

  bool operator==(const Managed &other) const { return block_ == other.block_; }
  bool operator!=(const Managed &other) const { return block_ != other.block_; }

private:
  struct ControlBlock
  {
    ControlBlock(T *ptr, ReleaserFn rel, void *ud)
        : value(ptr), releaser(rel), userData(ud), refCount(1)
    {
    }
    T *value;
    ReleaserFn releaser;
    void *userData;
    int refCount;
  };

  explicit Managed(ControlBlock *block) : block_(block) {}

  void retain()
  {
    if (block_)
      ++block_->refCount;
  }
  void release()
  {
    if (!block_)
      return;
    --block_->refCount;
    if (block_->refCount == 0)
    {
      if (block_->releaser)
        block_->releaser(block_->value, block_->userData);
      delete block_;
    }
    block_ = 0;
  }

  static void DefaultDelete(T *value, void *)
  {
    delete value;
  }

  ControlBlock *block_;
};

#endif // LOKA_CORE_MANAGED_HPP
