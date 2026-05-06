#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_OBSERVED_STATE_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_OBSERVED_STATE_HPP

#include <vector>
#include "app/scene/Node.hpp"
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class BoundaryObservedStateTestAccess;
    }
  } // namespace dsl

  namespace app
  {
    namespace scene
    {
      class BoundaryNode;

      struct BoundaryObservedStateBinding
      {
        BoundaryObservedStateBinding()
            : boundary(0),
              state(0),
              flags(NODE_DIRTY_NONE),
              refs(1),
              stateLifetimeToken(0)
        {
        }

        ~BoundaryObservedStateBinding()
        {
          if (stateLifetimeToken)
          {
            loka::core::StateBase::releaseExternalLifetimeToken(stateLifetimeToken);
            stateLifetimeToken = 0;
          }
        }

        void retain()
        {
          ++refs;
        }

        bool release()
        {
          --refs;
          return refs == 0;
        }

        BoundaryNode *boundary;
        loka::core::StateBase *state;
        NodeDirtyFlags flags;
        int refs;
        void *stateLifetimeToken;
      };

      struct BoundaryObservedStateEntry
      {
        BoundaryObservedStateEntry()
            : state(0),
              flags(NODE_DIRTY_NONE),
              observedGeneration(0),
              binding(0)
        {
        }
        loka::core::StateBase *state;
        NodeDirtyFlags flags;
        unsigned long observedGeneration;
        BoundaryObservedStateBinding *binding;
      };

      struct BoundaryObservedState
      {
        struct ObservedPassState
        {
          ObservedPassState()
              : generation(0)
          {
          }

          void begin()
          {
            ++generation;
            if (generation == 0)
            {
              generation = 1;
            }
          }

          bool ownsEntry(const BoundaryObservedStateEntry &entry) const
          {
            return entry.observedGeneration == generation;
          }

          unsigned long generation;
        };

        struct ObservedDirtyState
        {
          ObservedDirtyState()
              : dirtyFlags(NODE_DIRTY_NONE)
          {
          }

          void clear()
          {
            dirtyFlags = NODE_DIRTY_NONE;
          }

          void include(NodeDirtyFlags flagsToAdd)
          {
            if (flagsToAdd == NODE_DIRTY_NONE)
            {
              return;
            }
            dirtyFlags = static_cast<NodeDirtyFlags>(dirtyFlags | flagsToAdd);
          }

          NodeDirtyFlags value() const
          {
            return dirtyFlags;
          }

          NodeDirtyFlags dirtyFlags;
        };

        BoundaryObservedState()
            : pass(),
              dirty(),
              entries()
        {
        }

        void clearDirtyFlags()
        {
          dirty.clear();
        }

        NodeDirtyFlags currentDirtyFlags() const
        {
          return dirty.value();
        }

        void clearEntries(void (*changedThunk)(void *))
        {
          for (size_t i = 0; i < entries.size(); ++i)
          {
            BoundaryObservedStateEntry &entry = entries[i];
            if (entry.state && entry.binding)
            {
              if (entry.binding->stateLifetimeToken
                  && loka::core::StateBase::isExternalLifetimeTokenAlive(entry.binding->stateLifetimeToken))
              {
                entry.state->unbind(changedThunk, entry.binding);
              }
              entry.binding->boundary = 0;
              entry.binding->state = 0;
              entry.binding->flags = NODE_DIRTY_NONE;
              if (entry.binding->release())
              {
                delete entry.binding;
              }
              entry.binding = 0;
            }
          }
          entries.clear();
        }

        void beginPass()
        {
          pass.begin();
          for (size_t i = 0; i < entries.size(); ++i)
          {
            entries[i].flags = NODE_DIRTY_NONE;
            if (entries[i].binding)
            {
              entries[i].binding->flags = NODE_DIRTY_NONE;
            }
          }
        }

        void addDirtyFlags(NodeDirtyFlags flagsToAdd)
        {
          dirty.include(flagsToAdd);
        }

        void registerState(BoundaryNode *boundary,
                           loka::core::StateBase *state,
                           NodeDirtyFlags flagsToAdd,
                           void (*changedThunk)(void *))
        {
          if (!boundary || !state || flagsToAdd == NODE_DIRTY_NONE)
          {
            return;
          }
          addDirtyFlags(flagsToAdd);
          for (size_t i = 0; i < entries.size(); ++i)
          {
            if (entries[i].state == state)
            {
              entries[i].observedGeneration = pass.generation;
              entries[i].flags = static_cast<NodeDirtyFlags>(entries[i].flags | flagsToAdd);
              if (entries[i].binding)
              {
                entries[i].binding->flags = entries[i].flags;
                entries[i].binding->state = state;
                entries[i].binding->boundary = boundary;
              }
              return;
            }
          }
          BoundaryObservedStateEntry entry;
          entry.state = state;
          entry.flags = flagsToAdd;
          entry.observedGeneration = pass.generation;
          entry.binding = new BoundaryObservedStateBinding();
          entry.binding->boundary = boundary;
          entry.binding->state = state;
          entry.binding->flags = flagsToAdd;
          entry.binding->stateLifetimeToken = state->retainExternalLifetimeToken();
          state->bind(changedThunk, entry.binding, false, false, 0);
          entries.push_back(entry);
        }

        NodeDirtyFlags dirtyFlagsForCommittedStates(const loka::core::PushStateTracker *pushTracker) const
        {
          NodeDirtyFlags flags = NODE_DIRTY_NONE;
          if (!pushTracker)
          {
            return flags;
          }
          const std::vector<loka::core::StateBase *> &dirtyStates = pushTracker->committedDirtyStates();
          for (size_t stateIndex = 0; stateIndex < dirtyStates.size(); ++stateIndex)
          {
            loka::core::StateBase *dirtyState = dirtyStates[stateIndex];
            for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
            {
              if (entries[entryIndex].state == dirtyState)
              {
                if (!pass.ownsEntry(entries[entryIndex]))
                {
                  continue;
                }
                flags = static_cast<NodeDirtyFlags>(flags | entries[entryIndex].flags);
              }
            }
          }
          return flags;
        }

      private:
        ObservedPassState pass;
        ObservedDirtyState dirty;
        std::vector<BoundaryObservedStateEntry> entries;

        friend class ::loka::dsl::testing::BoundaryObservedStateTestAccess;
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_OBSERVED_STATE_HPP
