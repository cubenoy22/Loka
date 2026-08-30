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

      /** Allocation-free fingerprint of one seat's ordered arm roots. */
      struct BoundaryBranchSeatShape
      {
        BoundaryBranchSeatShape()
            : armCount(0),
              orderedRootTypes(2166136261UL)
        {
        }

        void append(const void *propsTypeId)
        {
          const unsigned char *bytes =
              reinterpret_cast<const unsigned char *>(&propsTypeId);
          for (size_t i = 0; i < sizeof(propsTypeId); ++i)
          {
            this->orderedRootTypes ^= bytes[i];
            this->orderedRootTypes *= 16777619UL;
          }
          ++this->armCount;
        }

        bool matches(const BoundaryBranchSeatShape &other) const
        {
          return this->armCount == other.armCount &&
                 this->orderedRootTypes == other.orderedRootTypes;
        }

        unsigned armCount;
        unsigned long orderedRootTypes;
      };

      struct BoundaryBranchSeatPlanEntry
      {
        BoundaryBranchSeatPlanEntry()
            : key(),
              dirtySource(0),
              seat(0),
              selectedArm(0),
              hasSelectedArm(false),
              shape(),
              hasOwner(false),
              ownerKey(),
              ownerArm(0)
        {
        }

        BoundaryBranchPlanBranch branch(unsigned arm) const
        {
          BoundaryBranchPlanBranch result;
          if (!this->seat || arm >= this->shape.armCount)
          {
            return result;
          }
          result.definition = this->seat->armDefinition(arm);
          IBranchPolicyScopeDefinition *scope =
              result.definition
                  ? result.definition->asBranchPolicyScopeDefinition()
                  : 0;
          if (scope)
          {
            result.definition = scope->scopedBranchDefinition();
            result.policies = scope->branchPolicies();
          }
          return result;
        }

        void snapshotSelection()
        {
          unsigned arm = 0;
          const bool selected = this->seat && this->seat->selectArm(arm);
          assert((!selected || arm < this->shape.armCount) &&
                 "branch seat selected an arm outside its declared arm count");
          this->hasSelectedArm = selected && arm < this->shape.armCount;
          this->selectedArm = this->hasSelectedArm ? arm : 0;
        }

        /** Canonicalizes a null branch to the caller-owned empty definition. */
        NodeDefinitionBase *materializedBranchDefinition(
            NodeDefinitionBase &emptyDefinition) const
        {
          NodeDefinitionBase *definition = this->hasSelectedArm
                                               ? this->branch(this->selectedArm).definition
                                               : 0;
          return definition ? definition : &emptyDefinition;
        }

        BoundaryParkedBranchKey key;
        loka::core::StateBase *dirtySource;
        IBranchSeatDefinition *seat;
        unsigned selectedArm;
        bool hasSelectedArm;
        BoundaryBranchSeatShape shape;
        bool hasOwner;
        BoundaryParkedBranchKey ownerKey;
        unsigned ownerArm;
      };

      struct BoundaryBranchSeatRuntimeEntry
      {
        BoundaryBranchSeatRuntimeEntry(const BoundaryParkedBranchKey &keyValue,
                                       Node *parentValue,
                                       Node *activeValue,
                                       unsigned armValue,
                                       bool hasActiveArmValue,
                                       const BoundaryBranchSeatShape &shapeValue,
                                       bool hasOwnerValue,
                                       const BoundaryParkedBranchKey &ownerKeyValue,
                                       unsigned ownerArmValue)
            : key(keyValue),
              parent(parentValue),
              active(activeValue),
              activeArm(armValue),
              hasActiveArm(hasActiveArmValue),
              shape(shapeValue),
              hasOwner(hasOwnerValue),
              ownerKey(ownerKeyValue),
              ownerArm(ownerArmValue),
              appliedGeneration(0)
        {
        }

        BoundaryParkedBranchKey key;
        /** Borrowed runtime parent; the Boundary owns both parent and child. */
        Node *parent;
        Node *active;
        unsigned activeArm;
        bool hasActiveArm;
        BoundaryBranchSeatShape shape;
        bool hasOwner;
        BoundaryParkedBranchKey ownerKey;
        unsigned ownerArm;
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
                Node *activeValue)
              : plan(planValue),
                parent(parentValue),
                active(activeValue)
          {
          }

          BoundaryBranchSeatPlanEntry plan;
          Node *parent;
          Node *active;
        };

        void record(const BoundaryBranchSeatPlanEntry &plan,
                    Node *parent,
                    Node *active)
        {
          this->entries_.push_back(Entry(plan, parent, active));
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
          this->captureDefinition(root, 0, 0);
          this->refuseCollidingKeys();
        }

        void append(NodeDefinitionBase *root)
        {
          this->captureDefinition(root, 0, 0);
          this->refuseCollidingKeys();
        }

        /** Tagged seat keys ignore the slot, so two distinct definitions under
            one key would share a single parked-ledger row. The walk records
            both; this pass drops every plan for such a key, and the plan-less
            seats are refused at materialization (requiresBoundaryPlan). The
            multiplicity in plans_ is the whole record -- no flag survives it. */
        void refuseCollidingKeys()
        {
          for (size_t i = 0; i < this->plans_.size();)
          {
            bool collided = false;
            for (size_t j = i + 1; j < this->plans_.size(); ++j)
            {
              if (this->plans_[j].key.matches(this->plans_[i].key) &&
                  this->plans_[j].seat != this->plans_[i].seat)
              {
                collided = true;
                break;
              }
            }
            if (!collided)
            {
              ++i;
              continue;
            }
            assert(false &&
                   "two branch seats share one tagged key: sibling branch seats "
                   "and BoundarySections require unique value keys");
            const BoundaryParkedBranchKey key = this->plans_[i].key;
            for (size_t k = 0; k < this->plans_.size();)
            {
              if (this->plans_[k].key.matches(key))
              {
                this->plans_.erase(this->plans_.begin() + k);
              }
              else
              {
                ++k;
              }
            }
          }
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
                             Node *active)
        {
          BoundaryBranchSeatRuntimeEntry *existing = this->findRuntime(plan.key);
          if (existing)
          {
            existing->parent = parent;
            existing->active = active;
            existing->activeArm = plan.selectedArm;
            existing->hasActiveArm = plan.hasSelectedArm;
            existing->shape = plan.shape;
            existing->hasOwner = plan.hasOwner;
            existing->ownerKey = plan.ownerKey;
            existing->ownerArm = plan.ownerArm;
            existing->appliedGeneration = this->generation_;
            return;
          }
          this->runtime_.push_back(
              BoundaryBranchSeatRuntimeEntry(plan.key,
                                             parent,
                                             active,
                                             plan.selectedArm,
                                             plan.hasSelectedArm,
                                             plan.shape,
                                             plan.hasOwner,
                                             plan.ownerKey,
                                             plan.ownerArm));
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
          return owner && owner->hasActiveArm &&
                 owner->activeArm == entry.ownerArm && this->isLive(*owner);
        }

        /** Removes a seat mapping only when its active branch reaches the
            structural detach/retire commit point. */
        bool eraseRuntimeForActive(Node *active,
                                   BoundaryParkedBranchKey &key,
                                   unsigned &arm,
                                   bool &hasActiveArm,
                                   unsigned &armCount)
        {
          for (size_t i = 0; i < this->runtime_.size(); ++i)
          {
            if (this->runtime_[i].active != active)
            {
              continue;
            }
            key = this->runtime_[i].key;
            arm = this->runtime_[i].activeArm;
            hasActiveArm = this->runtime_[i].hasActiveArm;
            armCount = this->runtime_[i].shape.armCount;
            this->runtime_.erase(this->runtime_.begin() + i);
            return true;
          }
          return false;
        }

        /** Removes one directly owned runtime mapping. The Boundary repeats
            this door while retiring the mapping's active subtree and every
            parked resident under the same seat key. */
        bool eraseOneOwnedRuntime(const BoundaryParkedBranchKey &ownerKey,
                                  unsigned ownerArm,
                                  BoundaryParkedBranchKey &erasedKey,
                                  unsigned &erasedArmCount)
        {
          for (size_t i = 0; i < this->runtime_.size(); ++i)
          {
            if (this->runtime_[i].hasOwner &&
                this->runtime_[i].ownerKey.matches(ownerKey) &&
                this->runtime_[i].ownerArm == ownerArm)
            {
              erasedKey = this->runtime_[i].key;
              erasedArmCount = this->runtime_[i].shape.armCount;
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
                               unsigned ownerArm)
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
                                    ownerArm);
            return;
          }

          IBranchSeatDefinition *seat = definition->asBranchSeatDefinition();
          if (seat)
          {
            const BoundaryParkedBranchKey key = keyFor(*definition, *seat);
            BoundaryBranchSeatPlanEntry *existing = this->findPlan(key);
            if (existing && existing->seat == seat)
            {
              // A nested append re-walks subtrees the outer capture already
              // covered: the first entry for the same definition stands.
              return;
            }
            // A different definition under an existing key is recorded as a
            // second entry; refuseCollidingKeys() drops every plan for that
            // key once the walk is complete, so both seats materialize as seats
            // without a captured plan.
            BoundaryBranchSeatPlanEntry entry;
            entry.key = key;
            entry.dirtySource = seat->branchCondition();
            entry.seat = seat;
            for (unsigned arm = 0; arm < seat->armCount(); ++arm)
            {
              BoundaryBranchPlanBranch branch = foldBranchRoot(seat->armDefinition(arm));
              entry.shape.append(branch.definition
                                     ? branch.definition->propsBase()->propsTypeId()
                                     : 0);
            }
            entry.snapshotSelection();
            if (ownerKey)
            {
              entry.hasOwner = true;
              entry.ownerKey = *ownerKey;
              entry.ownerArm = ownerArm;
            }
            this->plans_.push_back(entry);
            const BoundaryParkedBranchKey storedKey = entry.key;
            for (unsigned arm = 0; arm < entry.shape.armCount; ++arm)
            {
              this->captureDefinition(entry.branch(arm).definition, &storedKey, arm);
            }
            return;
          }

          INestableDefinition *nestable = definition->asNestableDefinition();
          if (!nestable)
          {
            return;
          }
          for (NodeDefinitionBase *child = nestable->childrenHead(); child; child = child->nextInComposition)
          {
            this->captureDefinition(child, ownerKey, ownerArm);
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
                                entry.active);
        }
        this->entries_.clear();
      }
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP
