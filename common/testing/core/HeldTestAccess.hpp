#ifndef LOKA_CORE_TESTING_HELD_TEST_ACCESS_HPP
#define LOKA_CORE_TESTING_HELD_TEST_ACCESS_HPP

#include "core/Held.hpp"

/** Testing-only readers over Held blocks and hold ledgers. This is core
    vocabulary: it lives beside the core type it opens, not inside whichever
    diagnostic happened to need it first. */
namespace loka
{
  namespace core
  {
    namespace testing
    {
      struct HeldTestAccess
      {
        static const loka::core::detail::HoldSlot *firstSlot(
            const loka::core::HoldLedger &ledger)
        {
          return ledger.head_;
        }

        static const loka::core::detail::HoldSlot *nextSlot(
            const loka::core::detail::HoldSlot *slot)
        {
          return slot ? slot->next : 0;
        }

        static const loka::core::detail::HeldBlockBase *block(
            const loka::core::detail::HoldSlot *slot)
        {
          return slot ? slot->block : 0;
        }

        template <typename T>
        static unsigned slotCount(const loka::core::Held<T> &held)
        {
          return held.block_ ? held.block_->activeSlotCount() : 0;
        }

        template <typename T>
        static unsigned holdCountForOwner(
            const loka::core::Held<T> &held,
            loka::app::scene::IStateOwner *owner)
        {
          if (!held.block_)
          {
            return 0;
          }
          for (unsigned i = 0;
               i < loka::core::detail::HeldBlockBase::kSlotCapacity;
               ++i)
          {
            if (held.block_->slotOwner(i) == owner)
            {
              return held.block_->slotHoldCount(i);
            }
          }
          return 0;
        }

        template <typename T>
        static unsigned slotHoldCount(const loka::core::Held<T> &held,
                                      unsigned index)
        {
          return held.block_ ? held.block_->slotHoldCount(index) : 0;
        }

        template <typename T>
        static loka::app::scene::IStateOwner *creator(
            const loka::core::Held<T> &held)
        {
          return held.block_ ? held.block_->creator() : 0;
        }

        template <typename T>
        static const void *blockAddress(const loka::core::Held<T> &held)
        {
          return held.block_;
        }

        template <typename T>
        static bool released(const loka::core::Held<T> &held)
        {
          return held.block_ && held.block_->released();
        }
      };
    } // namespace testing
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_TESTING_HELD_TEST_ACCESS_HPP
