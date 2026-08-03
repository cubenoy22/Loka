#ifndef LOKA_CORE_HELD_HPP
#define LOKA_CORE_HELD_HPP

#include <cassert>
#include <cstddef>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class IStateOwner;
      struct NodeComposition;
    } // namespace scene
  } // namespace app

  namespace core
  {
    class HoldLedger;

    namespace testing
    {
      struct HeldTestAccess;
    } // namespace testing

    namespace detail
    {
      class HeldBlockBase;

      enum HoldAttempt
      {
        HOLD_ATTEMPT_ACCEPTED = 0,
        HOLD_ATTEMPT_INVALID,
        HOLD_ATTEMPT_SLOT_OVERFLOW,
        HOLD_ATTEMPT_OUTSIDE_CREATOR_SUBTREE
      };

      struct HoldSlot
      {
        HoldSlot()
            : ownerKey(0),
              count(0),
              previous(0),
              next(0),
              block(0)
        {
        }

        loka::app::scene::IStateOwner *ownerKey;
        unsigned count;
        HoldSlot *previous;
        HoldSlot *next;
        HeldBlockBase *block;
      };

      /** Type-erased owner-slot ledger record embedded in a typed Held block.
          Slots carry their own intrusive owner-ledger links because one block
          may appear in four different owner scopes at once. */
      class HeldBlockBase
      {
      public:
        enum
        {
          kSlotCapacity = 4
        };

        typedef void (*ReleaseThunk)(HeldBlockBase *block);
        typedef void (*DestroyThunk)(void *storage);

        HeldBlockBase(loka::app::scene::IStateOwner *creator,
                      ReleaseThunk release,
                      DestroyThunk destroy)
            : creator_(creator),
              release_(release),
              destroy_(destroy),
              activeSlots_(0),
              releaseQueued_(false),
              released_(false),
              retireNext_(0),
              arenaNext_(0)
        {
          for (unsigned i = 0; i < kSlotCapacity; ++i)
          {
            this->slots_[i].block = this;
          }
        }

        ~HeldBlockBase()
        {
#ifdef LOKA_LIFECYCLE_AUDIT
          assert(this->activeSlots_ == 0 &&
                 "Held block storage reclaimed with live owner slots");
          assert(this->released_ &&
                 "Held block storage reclaimed before its releaser ran");
#endif
        }

        loka::app::scene::IStateOwner *creator() const
        {
          return this->creator_;
        }

        unsigned activeSlotCount() const
        {
          return this->activeSlots_;
        }

        loka::app::scene::IStateOwner *slotOwner(unsigned index) const
        {
          return index < kSlotCapacity ? this->slots_[index].ownerKey : 0;
        }

        unsigned slotHoldCount(unsigned index) const
        {
          return index < kSlotCapacity ? this->slots_[index].count : 0;
        }

        bool releaseQueued() const
        {
          return this->releaseQueued_;
        }

        bool released() const
        {
          return this->released_;
        }

        HeldBlockBase *retireNext() const
        {
          return this->retireNext_;
        }

        void setRetireNext(HeldBlockBase *next)
        {
          this->retireNext_ = next;
        }

        HeldBlockBase *arenaNext() const
        {
          return this->arenaNext_;
        }

        void setArenaNext(HeldBlockBase *next)
        {
          this->arenaNext_ = next;
        }

        void markReleaseQueued()
        {
          assert(!this->releaseQueued_ && !this->released_ &&
                 "Held releaser may be queued only once");
          this->releaseQueued_ = true;
        }

        void runReleaser()
        {
          assert(this->activeSlots_ == 0 && this->releaseQueued_ &&
                 "Held releaser requires an unowned queued block");
          if (this->released_)
          {
            return;
          }
          this->released_ = true;
          if (this->release_)
          {
            this->release_(this);
          }
        }

        void destroyStorage()
        {
          assert(this->destroy_ && "Held block requires a storage destructor");
          this->destroy_(this);
        }

      private:
        friend class loka::core::HoldLedger;
        friend struct loka::core::testing::HeldTestAccess;

        loka::app::scene::IStateOwner *creator_;
        ReleaseThunk release_;
        DestroyThunk destroy_;
        HoldSlot slots_[kSlotCapacity];
        unsigned activeSlots_;
        bool releaseQueued_;
        bool released_;
        HeldBlockBase *retireNext_;
        HeldBlockBase *arenaNext_;
      };

      template <typename T> class HeldBlock : public HeldBlockBase
      {
      public:
        typedef void (*ReleaserFn)(T *value);

        HeldBlock(loka::app::scene::IStateOwner *creator,
                  T *value,
                  ReleaserFn releaser)
            : HeldBlockBase(creator, &HeldBlock::Release, &HeldBlock::Destroy),
              value_(value),
              releaser_(releaser)
        {
        }

        T *value() const
        {
          return this->value_;
        }

      private:
        static void Release(HeldBlockBase *base)
        {
          HeldBlock *block = static_cast<HeldBlock *>(base);
          T *value = block->value_;
          block->value_ = 0;
          if (block->releaser_)
          {
            block->releaser_(value);
          }
        }

        static void Destroy(void *storage)
        {
          HeldBlock *block = static_cast<HeldBlock *>(storage);
          block->~HeldBlock();
        }

        T *value_;
        ReleaserFn releaser_;
      };
    } // namespace detail

    /** Intrusive list of the Held blocks owned by one lifecycle scope. */
    class HoldLedger
    {
    public:
      explicit HoldLedger(loka::app::scene::IStateOwner *owner);
      ~HoldLedger();

      detail::HoldAttempt tryHold(detail::HeldBlockBase *block);
      void dropAll();
      bool empty() const;
      void auditEmptyBeforeReclaim() const;
      void attachEnclosingOwner(loka::app::scene::IStateOwner *owner);

    private:
      friend struct testing::HeldTestAccess;

      HoldLedger(const HoldLedger &);
      HoldLedger &operator=(const HoldLedger &);

      bool ownerIsInsideCreatorSubtree(
          loka::app::scene::IStateOwner *creator) const;

      loka::app::scene::IStateOwner *owner_;
      detail::HoldSlot *head_;
      loka::app::scene::IStateOwner *enclosingOwner_;
    };

    /** Read-only view of a passive payload held by explicit owner scopes.
        Copying this handle never changes ownership; the holding owner's slot
        is the lifetime edge. */
    template <typename T> class Held
    {
    public:
      typedef void (*ReleaserFn)(T *value);

      Held()
          : block_(0)
      {
      }

      Held(const Held &other)
          : block_(other.block_)
      {
      }

      Held &operator=(const Held &other)
      {
        if (this != &other)
        {
          this->block_ = other.block_;
        }
        return *this;
      }

      T *get() const
      {
        return this->block_ ? this->block_->value() : 0;
      }

      T *operator->() const
      {
        return this->get();
      }

      T &operator*() const
      {
        assert(this->get() && "Held::operator* requires a live payload");
        return *this->get();
      }

      bool isValid() const
      {
        return this->get() != 0;
      }

      bool operator==(const Held &other) const
      {
        return this->block_ == other.block_;
      }

      bool operator!=(const Held &other) const
      {
        return this->block_ != other.block_;
      }

    private:
      friend struct loka::app::scene::NodeComposition;
      friend struct testing::HeldTestAccess;

      explicit Held(detail::HeldBlock<T> *block)
          : block_(block)
      {
      }

      static Held Create(loka::app::scene::IStateOwner *owner,
                         T *value,
                         ReleaserFn releaser);

      detail::HeldBlock<T> *block_;
    };
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_HELD_HPP
