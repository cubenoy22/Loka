#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP

#include <cassert>
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

        bool isLive(const BoundaryBranchSeatRuntimeEntry &entry) const
        {
          if (!entry.hasOwner)
          {
            return true;
          }
          const BoundaryBranchSeatRuntimeEntry *owner = this->findRuntime(entry.ownerKey);
          return owner && owner->activeCondition == entry.ownerCondition && this->isLive(*owner);
        }

        void eraseOwnedBranch(const BoundaryParkedBranchKey &ownerKey, bool ownerCondition)
        {
          bool erased;
          do
          {
            erased = false;
            for (size_t i = 0; i < this->runtime_.size(); ++i)
            {
              if (this->runtime_[i].hasOwner &&
                  this->runtime_[i].ownerKey.matches(ownerKey) &&
                  this->runtime_[i].ownerCondition == ownerCondition)
              {
                const BoundaryParkedBranchKey erasedKey = this->runtime_[i].key;
                this->runtime_.erase(this->runtime_.begin() + i);
                this->eraseOwnedBranch(erasedKey, false);
                this->eraseOwnedBranch(erasedKey, true);
                erased = true;
                break;
              }
            }
          } while (erased);
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
          assert(!definition->asBranchPolicyScopeDefinition() &&
                 "PolicyScope is legal only as the immediate branch root of a conditional seat");

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
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP
