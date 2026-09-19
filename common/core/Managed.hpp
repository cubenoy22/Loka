#ifndef LOKA_CORE_MANAGED_HPP
#define LOKA_CORE_MANAGED_HPP

#include <cstddef>
#include "core/LokaAlloc.hpp"

namespace loka
{
  namespace core
  {
    // Managed<T>: Simple intrusive reference-counted handle for platform resources.
    // - Wraps a pointer to T plus an optional releaser callback.
    // - Copying increments the refcount, destruction decrements it.
    // - When the count reaches zero, the releaser is invoked and the control block is destroyed.
    template <typename T> class Managed
    {
    public:
      typedef void (*ReleaserFn)(T *value, void *userData);

      Managed()
          : block_(0)
      {
      }
      Managed(const Managed &other)
          : block_(other.block_)
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

      /**
       * Adopts value only on success. On refusal the caller still owns value;
       * no releaser is invoked. The site's tag strings must outlive the block,
       * and the allocation backend must remain installed until its release.
       */
      static Managed<T> TryWrap(T *value, ReleaserFn releaser, void *userData, const LokaAllocationSite &site)
      {
        if (!value)
          return Managed<T>();
        void *storage = LokaAllocRaw(sizeof(GatedControlBlock), site);
        if (!storage)
          return Managed<T>();
        return Managed<T>(new (storage) GatedControlBlock(value, releaser, userData, site));
      }

      T *get() const
      {
        return block_ ? block_->value : 0;
      }
      T &operator*() const
      {
        return *block_->value;
      }
      T *operator->() const
      {
        return block_ ? block_->value : 0;
      }
      bool isValid() const
      {
        return block_ && block_->value;
      }
      int useCount() const
      {
        return block_ ? block_->refCount : 0;
      }

      void reset()
      {
        release();
        block_ = 0;
      }

      bool operator==(const Managed &other) const
      {
        return block_ == other.block_;
      }
      bool operator!=(const Managed &other) const
      {
        return block_ != other.block_;
      }

    private:
      struct ControlBlock
      {
        ControlBlock(T *ptr, ReleaserFn rel, void *ud)
            : value(ptr),
              releaser(rel),
              userData(ud),
              refCount(1),
              destroy(&Managed::DeleteControlBlock)
        {
        }
        T *value;
        ReleaserFn releaser;
        void *userData;
        int refCount;
        void (*destroy)(ControlBlock *);
      };

      struct GatedControlBlock : ControlBlock
      {
        GatedControlBlock(T *ptr, ReleaserFn rel, void *ud, const LokaAllocationSite &allocationSite)
            : ControlBlock(ptr, rel, ud),
              site(allocationSite)
        {
          this->destroy = &Managed::FreeControlBlock;
        }
        const LokaAllocationSite site;
      };

      static void DeleteControlBlock(ControlBlock *block)
      {
        delete block;
      }

      static void FreeControlBlock(ControlBlock *block)
      {
        GatedControlBlock *gated = static_cast<GatedControlBlock *>(block);
        const LokaAllocationSite site = gated->site;
        gated->~GatedControlBlock();
        LokaFreeRaw(gated, site);
      }

      explicit Managed(ControlBlock *block)
          : block_(block)
      {
      }

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
          block_->destroy(block_);
        }
        block_ = 0;
      }

      static void DefaultDelete(T *value, void *)
      {
        delete value;
      }

      ControlBlock *block_;
    };
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_MANAGED_HPP
