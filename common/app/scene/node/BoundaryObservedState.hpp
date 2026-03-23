#ifndef LOKA_CORE2_SCENE_NODE_BOUNDARY_OBSERVED_STATE_HPP
#define LOKA_CORE2_SCENE_NODE_BOUNDARY_OBSERVED_STATE_HPP

#include <vector>
#include "../Node.hpp"
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class BoundaryNode;

      struct BoundaryObservedStateBinding
      {
        BoundaryObservedStateBinding() : boundary(0), state(0), flags(NODE_DIRTY_NONE) {}
        BoundaryNode *boundary;
        loka::core::StateBase *state;
        NodeDirtyFlags flags;
      };

      struct BoundaryObservedStateEntry
      {
        BoundaryObservedStateEntry() : state(0), flags(NODE_DIRTY_NONE), observedGeneration(0), binding(0) {}
        loka::core::StateBase *state;
        NodeDirtyFlags flags;
        unsigned long observedGeneration;
        BoundaryObservedStateBinding *binding;
      };

      struct BoundaryObservedState
      {
        BoundaryObservedState() : dirtyFlags(NODE_DIRTY_NONE), generation(0), entries() {}

        void clearEntries()
        {
          for (size_t i = 0; i < entries.size(); ++i)
          {
            BoundaryObservedStateEntry &entry = entries[i];
            if (entry.state && entry.binding)
            {
              entry.binding->boundary = 0;
              entry.binding->state = 0;
              entry.binding->flags = NODE_DIRTY_NONE;
              entry.binding = 0;
            }
          }
          entries.clear();
        }

        void beginPass()
        {
          ++generation;
          if (generation == 0)
          {
            generation = 1;
          }
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
          if (flagsToAdd == NODE_DIRTY_NONE)
          {
            return;
          }
          dirtyFlags = static_cast<NodeDirtyFlags>(dirtyFlags | flagsToAdd);
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
              entries[i].observedGeneration = generation;
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
          entry.observedGeneration = generation;
          entry.binding = new BoundaryObservedStateBinding();
          entry.binding->boundary = boundary;
          entry.binding->state = state;
          entry.binding->flags = flagsToAdd;
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
                if (entries[entryIndex].observedGeneration != generation)
                {
                  continue;
                }
                flags = static_cast<NodeDirtyFlags>(flags | entries[entryIndex].flags);
              }
            }
          }
          return flags;
        }

        NodeDirtyFlags dirtyFlags;
        unsigned long generation;
        std::vector<BoundaryObservedStateEntry> entries;
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_BOUNDARY_OBSERVED_STATE_HPP
