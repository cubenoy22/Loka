#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP

#include <cassert>
#ifndef NDEBUG
#include <cstdio>
#endif
#include <vector>
#include "app/scene/Node.hpp"
#include "app/scene/boundary/detail/BoundaryParkedBranchLedger.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct BoundaryBranchPlanBranch
      {
        BoundaryBranchPlanBranch()
            : definition(0),
              policies()
        {
        }

        NodeDefinitionBase *definition;
        BranchPolicies policies;
      };

      struct BoundaryBranchSeatPlanEntry
      {
        BoundaryBranchSeatPlanEntry()
            : key(),
              condition(0),
              whenFalse(),
              whenTrue(),
              hasOwner(false),
              ownerKey(),
              ownerCondition(false)
        {
        }

        const BoundaryBranchPlanBranch &branch(bool value) const
        {
          return value ? this->whenTrue : this->whenFalse;
        }

        /** Canonicalizes a null branch to the caller-owned empty definition. */
        NodeDefinitionBase *materializedBranchDefinition(
            bool value,
            NodeDefinitionBase &emptyDefinition) const
        {
          NodeDefinitionBase *definition = this->branch(value).definition;
          return definition ? definition : &emptyDefinition;
        }

        BoundaryParkedBranchKey key;
        loka::core::State<bool> *condition;
        BoundaryBranchPlanBranch whenFalse;
        BoundaryBranchPlanBranch whenTrue;
        bool hasOwner;
        BoundaryParkedBranchKey ownerKey;
        bool ownerCondition;
      };

      struct BoundaryBranchSeatRuntimeEntry
      {
        BoundaryBranchSeatRuntimeEntry(const BoundaryParkedBranchKey &keyValue,
                                       Node *parentValue,
                                       Node *activeValue,
                                       bool conditionValue,
                                       bool hasOwnerValue,
                                       const BoundaryParkedBranchKey &ownerKeyValue,
                                       bool ownerConditionValue)
            : key(keyValue),
              parent(parentValue),
              active(activeValue),
              activeCondition(conditionValue),
              hasOwner(hasOwnerValue),
              ownerKey(ownerKeyValue),
              ownerCondition(ownerConditionValue),
              appliedGeneration(0)
        {
        }

        BoundaryParkedBranchKey key;
        /** Borrowed runtime parent; the Boundary owns both parent and child. */
        Node *parent;
        Node *active;
        bool activeCondition;
        bool hasOwner;
        BoundaryParkedBranchKey ownerKey;
        bool ownerCondition;
        unsigned long appliedGeneration;
      };

      class BoundaryBranchSeatState;

      /** Uncommitted runtime-seat facts produced while a fallible local
          rebuild materializes candidates. Commit publishes them only after
          the structural plan has installed its new roots and removed stale
          mappings. */
      class BoundaryBranchSeatRuntimeRegistrationPlan
      {
      public:
        struct Entry
        {
          Entry(const BoundaryBranchSeatPlanEntry &planValue,
                Node *parentValue,
                Node *activeValue,
                bool conditionValue)
              : plan(planValue),
                parent(parentValue),
                active(activeValue),
                condition(conditionValue)
          {
          }

          BoundaryBranchSeatPlanEntry plan;
          Node *parent;
          Node *active;
          bool condition;
        };

        void record(const BoundaryBranchSeatPlanEntry &plan,
                    Node *parent,
                    Node *active,
                    bool condition)
        {
          this->entries_.push_back(Entry(plan, parent, active, condition));
        }

        void clear()
        {
          this->entries_.clear();
        }

        size_t count() const
        {
          return this->entries_.size();
        }

        void commitTo(BoundaryBranchSeatState &state);

      private:
        std::vector<Entry> entries_;
      };

      /** Boundary-owned definition plans and runtime seat ownership. Plans
          borrow the current composition generation; runtime entries borrow
          chain residents owned by this Boundary. */
      class BoundaryBranchSeatState
      {
      public:
        BoundaryBranchSeatState()
            : plans_(),
              runtime_(),
              generation_(0)
#ifndef NDEBUG
              , misplacementHintEmitted_(false)
#endif
        {
        }

        void capture(NodeDefinitionBase *root)
        {
          this->plans_.clear();
          ++this->generation_;
          this->captureDefinition(root, 0, false);
        }

        void append(NodeDefinitionBase *root)
        {
          this->captureDefinition(root, 0, false);
        }

        unsigned long generation() const
        {
          return this->generation_;
        }

        const std::vector<BoundaryBranchSeatPlanEntry> &plans() const
        {
          return this->plans_;
        }

        BoundaryBranchSeatPlanEntry *findPlan(const BoundaryParkedBranchKey &key)
        {
          for (size_t i = 0; i < this->plans_.size(); ++i)
          {
            if (this->plans_[i].key.matches(key))
            {
              return &this->plans_[i];
            }
          }
          return 0;
        }

        const BoundaryBranchSeatPlanEntry *findPlan(const BoundaryParkedBranchKey &key) const
        {
          return const_cast<BoundaryBranchSeatState *>(this)->findPlan(key);
        }

        BoundaryBranchSeatRuntimeEntry *findRuntime(const BoundaryParkedBranchKey &key)
        {
          for (size_t i = 0; i < this->runtime_.size(); ++i)
          {
            if (this->runtime_[i].key.matches(key))
            {
              return &this->runtime_[i];
            }
          }
          return 0;
        }

        const BoundaryBranchSeatRuntimeEntry *findRuntime(const BoundaryParkedBranchKey &key) const
        {
          return const_cast<BoundaryBranchSeatState *>(this)->findRuntime(key);
        }

        void registerRuntime(const BoundaryBranchSeatPlanEntry &plan,
                             Node *parent,
                             Node *active,
                             bool condition)
        {
          BoundaryBranchSeatRuntimeEntry *existing = this->findRuntime(plan.key);
          if (existing)
          {
            existing->parent = parent;
            existing->active = active;
            existing->activeCondition = condition;
            existing->hasOwner = plan.hasOwner;
            existing->ownerKey = plan.ownerKey;
            existing->ownerCondition = plan.ownerCondition;
            existing->appliedGeneration = this->generation_;
            return;
          }
          this->runtime_.push_back(
              BoundaryBranchSeatRuntimeEntry(plan.key,
                                             parent,
                                             active,
                                             condition,
                                             plan.hasOwner,
                                             plan.ownerKey,
                                             plan.ownerCondition));
          this->runtime_.back().appliedGeneration = this->generation_;
        }

        /** Performs the only potentially allocating part of publishing staged
            runtime facts while the local rebuild is still fallible. */
        void reserveRuntimeRegistrations(size_t additionalCount)
        {
          this->runtime_.reserve(this->runtime_.size() + additionalCount);
        }

        bool isLive(const BoundaryBranchSeatRuntimeEntry &entry) const
        {
          if (!entry.hasOwner)
          {
            return true;
          }
          const BoundaryBranchSeatRuntimeEntry *owner = this->findRuntime(entry.ownerKey);
          return owner && owner->activeCondition == entry.ownerCondition && this->isLive(*owner);
        }

        /** Removes a seat mapping only when its active branch reaches the
            structural detach/retire commit point. */
        bool eraseRuntimeForActive(Node *active,
                                   BoundaryParkedBranchKey &key,
                                   bool &condition)
        {
          for (size_t i = 0; i < this->runtime_.size(); ++i)
          {
            if (this->runtime_[i].active != active)
            {
              continue;
            }
            key = this->runtime_[i].key;
            condition = this->runtime_[i].activeCondition;
            this->runtime_.erase(this->runtime_.begin() + i);
            return true;
          }
          return false;
        }

        /** Removes one directly owned runtime mapping. The Boundary repeats
            this door while retiring the mapping's active subtree and every
            parked resident under the same seat key. */
        bool eraseOneOwnedRuntime(const BoundaryParkedBranchKey &ownerKey,
                                  bool ownerCondition,
                                  BoundaryParkedBranchKey &erasedKey)
        {
          for (size_t i = 0; i < this->runtime_.size(); ++i)
          {
            if (this->runtime_[i].hasOwner &&
                this->runtime_[i].ownerKey.matches(ownerKey) &&
                this->runtime_[i].ownerCondition == ownerCondition)
            {
              erasedKey = this->runtime_[i].key;
              this->runtime_.erase(this->runtime_.begin() + i);
              return true;
            }
          }
          return false;
        }

        void clearRuntime()
        {
          this->runtime_.clear();
        }

      private:
        static BoundaryParkedBranchKey keyFor(NodeDefinitionBase &definition,
                                              IBranchSeatDefinition &seat)
        {
          return BoundaryParkedBranchKey(definition.nodeTag(),
                                         definition.compositionSeatSlot(),
                                         seat.branchSeatTypeId());
        }

        static BoundaryBranchPlanBranch foldBranchRoot(NodeDefinitionBase *definition)
        {
          BoundaryBranchPlanBranch result;
          result.definition = definition;
          if (!definition)
          {
            return result;
          }

          IBranchPolicyScopeDefinition *scope =
              definition->asBranchPolicyScopeDefinition();
          if (scope)
          {
            result.definition = scope->scopedBranchDefinition();
            result.policies = scope->branchPolicies();
          }
          return result;
        }

        void captureDefinition(NodeDefinitionBase *definition,
                               const BoundaryParkedBranchKey *ownerKey,
                               bool ownerCondition)
        {
          if (!definition)
          {
            return;
          }
          IBranchPolicyScopeDefinition *scope =
              definition->asBranchPolicyScopeDefinition();
          if (scope)
          {
#ifndef NDEBUG
            if (!this->misplacementHintEmitted_)
            {
              std::fprintf(stderr,
                           "PolicyScope is not the sole branch root of its conditional seat; "
                           "its park policies are ignored. Place PolicyScope directly as the "
                           "branch root.\n");
              this->misplacementHintEmitted_ = true;
            }
#endif
            this->captureDefinition(scope->scopedBranchDefinition(),
                                    ownerKey,
                                    ownerCondition);
            return;
          }

          IBranchSeatDefinition *seat = definition->asBranchSeatDefinition();
          if (seat)
          {
            BoundaryBranchSeatPlanEntry entry;
            entry.key = keyFor(*definition, *seat);
            entry.condition = seat->branchCondition();
            entry.whenFalse = foldBranchRoot(seat->branchDefinition(false));
            entry.whenTrue = foldBranchRoot(seat->branchDefinition(true));
            if (ownerKey)
            {
              entry.hasOwner = true;
              entry.ownerKey = *ownerKey;
              entry.ownerCondition = ownerCondition;
            }
            this->plans_.push_back(entry);
            const BoundaryParkedBranchKey storedKey = entry.key;
            this->captureDefinition(entry.whenFalse.definition, &storedKey, false);
            this->captureDefinition(entry.whenTrue.definition, &storedKey, true);
            return;
          }

          INestableDefinition *nestable = definition->asNestableDefinition();
          if (!nestable)
          {
            return;
          }
          for (NodeDefinitionBase *child = nestable->childrenHead(); child; child = child->nextInComposition)
          {
            this->captureDefinition(child, ownerKey, ownerCondition);
          }
        }

        std::vector<BoundaryBranchSeatPlanEntry> plans_;
        std::vector<BoundaryBranchSeatRuntimeEntry> runtime_;
        unsigned long generation_;
#ifndef NDEBUG
        bool misplacementHintEmitted_;
#endif
      };

      inline void BoundaryBranchSeatRuntimeRegistrationPlan::commitTo(
          BoundaryBranchSeatState &state)
      {
        for (size_t i = 0; i < this->entries_.size(); ++i)
        {
          Entry &entry = this->entries_[i];
          state.registerRuntime(entry.plan,
                                entry.parent,
                                entry.active,
                                entry.condition);
        }
        this->entries_.clear();
      }
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP
