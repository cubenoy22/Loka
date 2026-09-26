#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP

#include <cassert>
#ifndef NDEBUG
#include <cstdio>
#endif
#include <vector>
#include "app/scene/Node.hpp"
#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/boundary/detail/BoundaryParkedBranchLedger.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class OwnershipDump;
    }
  }

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
        BoundaryBranchSeatPlanEntry(const BoundaryParkedBranchKey &keyValue)
            : key(keyValue),
              dirtySource(0),
              definition(0),
              selectedArm(0),
              hasSelectedArm(false),
              armCount(0),
              hasOwner(false),
              ownerKey(keyValue),
              ownerArm(0)
        {
        }

        BoundaryBranchPlanBranch branch(unsigned arm) const
        {
          BoundaryBranchPlanBranch result;
          if (!this->seat() || arm >= this->armCount)
          {
            return result;
          }
          result.definition = this->seat()->armDefinition(arm);
          IBranchPolicyScopeDefinition *scope =
              result.definition
                  ? result.definition->asBranchPolicyScopeDefinition()
                  : 0;
          if (scope)
          {
            result.definition = scope->scopedBranchDefinition();
            result.policies = scope->branchPolicies();
          }
          const BranchPolicies direct = this->seat()->armPolicies(arm);
          result.policies.destroyOnDetach = result.policies.destroyOnDetach || direct.destroyOnDetach;
          result.policies.deliverWhileDetached = result.policies.deliverWhileDetached || direct.deliverWhileDetached;
          return result;
        }

        void snapshotSelection()
        {
          unsigned arm = 0;
          const bool selected = this->seat() && this->seat()->selectArm(arm);
          assert((!selected || arm < this->armCount) &&
                 "branch seat selected an arm outside its declared arm count");
          this->hasSelectedArm = selected && arm < this->armCount;
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
        NodeDefinitionBase *definition;
        IBranchSeatDefinition *seat() const
        {
          return this->definition ? this->definition->asBranchSeatDefinition() : 0;
        }
        unsigned selectedArm;
        bool hasSelectedArm;
        unsigned armCount;
        bool hasOwner;
        BoundaryParkedBranchKey ownerKey;
        unsigned ownerArm;
      };

      struct BoundaryBranchSeatRuntimeEntry
      {
        BoundaryBranchSeatRuntimeEntry()
            : key(NODE_TAG_NONE, 0, 0, 0), parent(0), active(0), activeArm(0),
              hasActiveArm(false), armCount(0), hasOwner(false),
              ownerKey(NODE_TAG_NONE, 0, 0, 0), ownerArm(0), stateOwner(0)
        {
        }

        BoundaryBranchSeatRuntimeEntry(const BoundaryParkedBranchKey &keyValue,
                                       Node *parentValue,
                                       Node *activeValue,
                                       unsigned armValue,
                                       bool hasActiveArmValue,
                                       unsigned armCountValue,
                                       bool hasOwnerValue,
                                       const BoundaryParkedBranchKey &ownerKeyValue,
                                       unsigned ownerArmValue,
                                       IStateOwner *stateOwnerValue)
            : key(keyValue),
              parent(parentValue),
              active(activeValue),
              activeArm(armValue),
              hasActiveArm(hasActiveArmValue),
              armCount(armCountValue),
              hasOwner(hasOwnerValue),
              ownerKey(ownerKeyValue),
              ownerArm(ownerArmValue),
              stateOwner(stateOwnerValue)
        {
        }

        BoundaryParkedBranchKey key;
        /** Borrowed runtime parent; the Boundary owns both parent and child. */
        Node *parent;
        Node *active;
        unsigned activeArm;
        bool hasActiveArm;
        unsigned armCount;
        bool hasOwner;
        BoundaryParkedBranchKey ownerKey;
        unsigned ownerArm;
        /** Borrowed from the runtime parent scope until this mapping retires. */
        IStateOwner *stateOwner;
      };

      class BranchSeatDeclaration;
      class BoundaryBranchSeatState;

      /** Uncommitted runtime-seat facts produced while a fallible local
          rebuild materializes candidates. Commit publishes them only after
          the structural plan has installed its new roots and removed stale
          mappings. Owns declarations until that same commit; clear and destruction
          dispose them, and appendTo transfers ownership to its enclosing plan. */
      class BoundaryBranchSeatRuntimeRegistrationPlan
      {
      public:
        BoundaryBranchSeatRuntimeRegistrationPlan();
        ~BoundaryBranchSeatRuntimeRegistrationPlan();

        /** Takes candidate only on success. One nullable allocation per declaration. */
        bool stageDeclaration(IBranchSeatDefinition *seat,
                              loka::core::OwnedDef<BranchSeatDeclaration> &candidate);

        struct Entry
        {
          Entry(const BoundaryBranchSeatPlanEntry &planValue,
                Node *parentValue,
                const NodeMaterializationResult &activeValue,
                IStateOwner *stateOwnerValue)
              : plan(planValue),
                parent(parentValue),
                active(activeValue),
                stateOwner(stateOwnerValue)
          {
          }

          BoundaryBranchSeatPlanEntry plan;
          Node *parent;
          NodeMaterializationResult active;
          IStateOwner *stateOwner;
        };

        void record(const BoundaryBranchSeatPlanEntry &plan, Node *parent,
                    const NodeMaterializationResult &active, IStateOwner *stateOwner)
        {
          this->entries_.push_back(Entry(plan, parent, active, stateOwner));
        }

        /** Transfers rows and declarations; the source becomes empty. */
        void appendTo(BoundaryBranchSeatRuntimeRegistrationPlan &target);

        void clear();

        size_t count() const
        {
          return this->entries_.size();
        }

        void commitTo(BoundaryBranchSeatState &state);

      private:
        /** Owns gate-allocated staging entries and their declaration payloads.
            Push and splice keep the owning head and borrowed tail consistent. */
        class StagedDeclarationChain
        {
        public:
          StagedDeclarationChain();
          ~StagedDeclarationChain();
          bool push(IBranchSeatDefinition *seat, loka::core::OwnedDef<BranchSeatDeclaration> &candidate);
          void spliceTo(StagedDeclarationChain &target);
          void clear();
          void commit();

        private:
          struct Entry;
          Entry *head_;
          Entry *tail_;
          StagedDeclarationChain(const StagedDeclarationChain &);
          StagedDeclarationChain &operator=(const StagedDeclarationChain &);
        };
        StagedDeclarationChain declarations_;
        std::vector<Entry> entries_;

        BoundaryBranchSeatRuntimeRegistrationPlan(const BoundaryBranchSeatRuntimeRegistrationPlan &);
        BoundaryBranchSeatRuntimeRegistrationPlan &operator=(const BoundaryBranchSeatRuntimeRegistrationPlan &);
      };

      /** Boundary-owned definition plans and runtime seat ownership. Plans
          borrow the current composition generation; runtime entries borrow
          chain residents owned by this Boundary. */
      class BoundaryBranchSeatState
      {
      public:
        BoundaryBranchSeatState()
            : plans_(),
              runtime_()
#ifndef NDEBUG
              , misplacementHintEmitted_(false)
#endif
        {
        }

        void capture(NodeDefinitionBase *root)
        {
          this->plans_.clear();
          this->captureDefinition(root, 0, 0);
          this->assertUniqueKeys();
        }

        void captureOwned(NodeDefinitionBase *root, const BoundaryParkedBranchKey &ownerKey, unsigned ownerArm)
        {
          this->plans_.clear();
          this->captureDefinition(root, &ownerKey, ownerArm);
          this->assertUniqueKeys();
        }

        void append(NodeDefinitionBase *root)
        {
          this->captureDefinition(root, 0, 0);
          this->assertUniqueKeys();
        }

        /** Misuse detection only: the release wall is findPlan() refusing a
            key with more than one distinct claimant. */
        void assertUniqueKeys() const
        {
#ifndef NDEBUG
          std::vector<IBranchSeatDefinition *> seats;
          seats.reserve(this->plans_.size());
          for (size_t i = 0; i < this->plans_.size(); ++i)
            seats.push_back(this->plans_[i].seat());
          for (size_t i = 0; i < this->plans_.size(); ++i)
          {
            for (size_t j = i + 1; j < this->plans_.size(); ++j)
            {
              assert(!(this->plans_[j].key.matches(this->plans_[i].key) && seats[j] != seats[i])
                     && "two branch seats share one tagged key: sibling branch seats "
                        "and BoundarySections require unique value keys");
            }
          }
#endif
        }

        const std::vector<BoundaryBranchSeatPlanEntry> &plans() const
        {
          return this->plans_;
        }

        /** The plan for a key, or 0 when no plan -- or more than one distinct
            definition -- claims it. Tagged seat keys ignore the slot, so two
            definitions under one key would share a single parked-ledger row;
            every claimant is recorded and the multiplicity itself is the
            refusal, so a later append() under the same key cannot revive it. */
        BoundaryBranchSeatPlanEntry *findPlan(const BoundaryParkedBranchKey &key)
        {
          assert(key.scope != 0);
          if (key.scope != this)
          {
            return key.scope->findPlan(key);
          }
          BoundaryBranchSeatPlanEntry *found = 0;
          for (size_t i = 0; i < this->plans_.size(); ++i)
          {
            if (!this->plans_[i].key.matches(key))
            {
              continue;
            }
            if (found && this->plans_[i].seat() != found->seat())
            {
              return 0;
            }
            if (!found)
            {
              found = &this->plans_[i];
            }
          }
          return found;
        }

        /** The entry recorded for exactly this definition under the key. */
        BoundaryBranchSeatPlanEntry *findPlanForSeat(const BoundaryParkedBranchKey &key,
                                                     const IBranchSeatDefinition *seat)
        {
          for (size_t i = 0; i < this->plans_.size(); ++i)
          {
            if (this->plans_[i].key.matches(key) && this->plans_[i].seat() == seat)
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

        /** Copies one row from this Boundary's ledger; leaves out unchanged on
            absence. The type prevents retaining a reference into the ledger,
            but does not protect the copy's freshness. Callers rely on synchronous
            admission: no shipping path changes the ledger between taking the
            copy and using its borrowed facts. */
        bool queryRuntime(const BoundaryParkedBranchKey &key,
                          BoundaryBranchSeatRuntimeEntry &out) const
        {
          const BoundaryBranchSeatRuntimeEntry *row = this->findRuntime(key);
          if (!row)
            return false;
          out = *row;
          return true;
        }

        /** Commits only the active-arm facts of an existing row, by key. */
        bool commitActiveArm(const BoundaryParkedBranchKey &key, Node *active,
                             const BoundaryBranchSeatPlanEntry &plan)
        {
          BoundaryBranchSeatRuntimeEntry *row = this->findRuntime(key);
          if (!row)
            return false;
          assert(row->armCount == plan.armCount);
          row->active = active;
          row->activeArm = plan.selectedArm;
          row->hasActiveArm = plan.hasSelectedArm;
          return true;
        }

        /** Vacates an existing row without changing its registration facts. */
        bool vacateActiveArm(const BoundaryParkedBranchKey &key)
        {
          BoundaryBranchSeatRuntimeEntry *row = this->findRuntime(key);
          if (!row)
            return false;
          row->active = 0;
          row->hasActiveArm = false;
          return true;
        }

        void registerRuntime(const BoundaryBranchSeatPlanEntry &plan,
                             Node *parent,
                             Node *active,
                             IStateOwner *stateOwner)
        {
          BoundaryBranchSeatRuntimeEntry *existing = this->findRuntime(plan.key);
          if (existing)
          {
            assert(existing->parent == parent);
            assert(existing->stateOwner == stateOwner);
            existing->active = active;
            existing->activeArm = plan.selectedArm;
            existing->hasActiveArm = plan.hasSelectedArm;
            existing->armCount = plan.armCount;
            existing->hasOwner = plan.hasOwner;
            existing->ownerKey = plan.ownerKey;
            existing->ownerArm = plan.ownerArm;
            return;
          }
          this->runtime_.push_back(BoundaryBranchSeatRuntimeEntry(plan.key,
                                                                  parent,
                                                                  active,
                                                                  plan.selectedArm,
                                                                  plan.hasSelectedArm,
                                                                  plan.armCount,
                                                                  plan.hasOwner,
                                                                  plan.ownerKey,
                                                                  plan.ownerArm,
                                                                  stateOwner));
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
            armCount = this->runtime_[i].armCount;
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
              erasedArmCount = this->runtime_[i].armCount;
              this->runtime_.erase(this->runtime_.begin() + i);
              return true;
            }
          }
          return false;
        }

        bool referencesScope(const BoundaryBranchSeatState *scope) const
        {
          for (size_t i = 0; i < this->runtime_.size(); ++i)
            if (this->runtime_[i].key.scope == scope || this->runtime_[i].ownerKey.scope == scope)
              return true;
          return false;
        }

        void eraseScopeRuntime(const BoundaryBranchSeatState *scope)
        {
          for (size_t i = this->runtime_.size(); i != 0; --i)
            if (this->runtime_[i - 1].key.scope == scope || this->runtime_[i - 1].ownerKey.scope == scope)
              this->runtime_.erase(this->runtime_.begin() + i - 1);
        }

        bool runtimeEmpty() const
        {
          return this->runtime_.empty();
        }

        void clearRuntime()
        {
          this->runtime_.clear();
        }

      private:
        friend class ::loka::dsl::testing::OwnershipDump;

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

        BoundaryParkedBranchKey keyFor(NodeDefinitionBase &definition, IBranchSeatDefinition &seat)
        {
          return BoundaryParkedBranchKey(
              definition.nodeTag(), definition.compositionSeatSlot(), seat.branchSeatTypeId(), this);
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
            if (this->findPlanForSeat(key, seat))
            {
              // A nested append re-walks subtrees the outer capture already
              // covered: the first entry for the same definition stands.
              return;
            }
            // A different definition under an existing key is recorded too:
            // findPlan() answers 0 for a key with more than one claimant, so
            // every seat under it materializes as a seat without a plan.
            BoundaryBranchSeatPlanEntry entry(key);
            entry.dirtySource = seat->branchCondition();
            entry.definition = definition;
            entry.armCount = seat->armCount();
            entry.snapshotSelection();
            if (ownerKey)
            {
              entry.hasOwner = true;
              entry.ownerKey = *ownerKey;
              entry.ownerArm = ownerArm;
            }
            this->plans_.push_back(entry);
            const BoundaryParkedBranchKey storedKey = entry.key;
            if (!seat->declaredBranchSeats())
            {
              // Fixed arms belong to this scope; declaring seats own their arms.
              for (unsigned arm = 0; arm < entry.armCount; ++arm)
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
#ifndef NDEBUG
        bool misplacementHintEmitted_;
#endif
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYBRANCHSEATSTATE_HPP
