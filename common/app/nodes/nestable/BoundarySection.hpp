#ifndef LOKA_APP2_NODES_NESTABLE_BOUNDARY_SECTION_HPP
#define LOKA_APP2_NODES_NESTABLE_BOUNDARY_SECTION_HPP

#include <cassert>
#include "app/scene/Node.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/boundary/BoundaryInnerStateOwner.hpp"

namespace loka
{
  namespace app
  {
    struct BoundarySectionTypeTag
    {
    };

    class BoundarySectionNode;

    /** Stable inputs for one explicit Boundary-local owner scope. A value key
        is mandatory because it names the runtime seat whose residents survive
        compatible recomposition. */
    struct SectionProps : public scene::NodePropsBase<SectionProps>
    {
      typedef BoundarySectionTypeTag TypeTag;
      typedef BoundarySectionNode NodeType;

      SectionProps()
          : key_(scene::NODE_TAG_NONE)
      {
        assert(false && "SectionProps requires a nonzero value key");
      }

      explicit SectionProps(scene::NodeTag key)
          : key_(key)
      {
        assert(this->key_ != scene::NODE_TAG_NONE &&
               "SectionProps requires a nonzero value key");
      }

      scene::NodeTag key() const
      {
        return this->key_;
      }

      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != this->propsTypeId())
        {
          return false;
        }
        const SectionProps &other = static_cast<const SectionProps &>(rhs);
        return this->key_ < other.key_;
      }

    private:
      scene::NodeTag key_;
    };

    /** Runtime owner-scope box inside a Boundary. Its logical ownership rows
        and tracker live here; state storage comes from the enclosing
        Boundary's StateArena and is returned there when this node is reclaimed. */
    class BoundarySectionNode : public scene::NestableNode,
                                public scene::BoundaryInnerStateOwner
    {
    public:
      typedef BoundarySectionTypeTag TypeTag;
      SectionProps props;

      explicit BoundarySectionNode(const SectionProps &p)
          : scene::NestableNode(),
            scene::BoundaryInnerStateOwner(),
            props(p)
      {
        assert(this->props.key() != scene::NODE_TAG_NONE &&
               "BoundarySectionNode requires a value key");
      }

      virtual ~BoundarySectionNode()
      {
        // Descendant registrations and observations must disappear before
        // their owner storage. Normal subtree retirement has already detached
        // children; this also preserves the wall for direct heap teardown.
        this->clearChildrenInternal(false);
        this->clearOwnedStates();
      }

      virtual scene::IStateOwner *asStateOwner()
      {
        return this;
      }

      virtual void attachEnclosingBoundary(scene::BoundaryNode *boundary)
      {
        scene::BoundaryInnerStateOwner::attachEnclosingBoundary(boundary);
        if (boundary)
        {
          this->setInvalidateTarget(boundary);
          this->setInvalidateCallback(
              &BoundarySectionNode::InvalidateEnclosingBoundaryThunk,
              this);
        }
      }

      virtual BoundarySectionNode *asBoundarySectionNode()
      {
        return this;
      }

      virtual const void *nodeTypeKey() const
      {
        return scene::NodeTypeToken<BoundarySectionNode>();
      }

      virtual void render(scene::IPlatformController *controller)
      {
        scene::Node *child = this->childrenHead();
        while (child)
        {
          child->render(controller);
          child = child->nextInComposition;
        }
      }

      virtual short layout(scene::IPlatformController *controller,
                           scene::LayoutState &state)
      {
        short result = 0;
        scene::Node *child = this->childrenHead();
        while (child)
        {
          result = child->layout(controller, state);
          child = child->nextInComposition;
        }
        return result;
      }

      virtual void noteStateAllocationFailure()
      {
        scene::BoundaryNode *boundary = this->requireEnclosingBoundary();
        if (boundary)
        {
          boundary->noteStateAllocationFailure();
        }
      }

      virtual void reserveStateArena(size_t totalSize)
      {
        scene::BoundaryNode *boundary = this->requireEnclosingBoundary();
        if (boundary)
        {
          boundary->reserveStateArena(totalSize);
        }
      }

      virtual void *allocateStateMemory(size_t size, size_t align)
      {
        scene::BoundaryNode *boundary = this->requireEnclosingBoundary();
        return boundary ? boundary->allocateStateMemory(size, align) : 0;
      }

      virtual void registerStateMemory(
          loka::core::StateBase *state,
          void (*destroy)(loka::core::StateBase *))
      {
        scene::BoundaryNode *boundary = this->requireEnclosingBoundary();
        if (boundary)
        {
          boundary->registerStateMemory(state, destroy);
        }
      }

    protected:
      virtual void detachOwnedStateFromAncestors(
          loka::core::StateBase *state)
      {
        scene::BoundaryNode *boundary = this->enclosingBoundary();
        if (boundary)
        {
          boundary->forgetInnerOwnedState(state);
        }
      }

      virtual void destroyOwnedStateStorage(loka::core::StateBase *state)
      {
        if (!state)
        {
          return;
        }
        if (state->isArenaAllocated())
        {
          scene::BoundaryNode *boundary = this->requireEnclosingBoundary();
          if (boundary)
          {
            boundary->releaseInnerArenaStateMemory(state);
          }
          return;
        }
        scene::DestroyAdoptedHeapState(state);
      }

    private:
      static void InvalidateEnclosingBoundaryThunk(void *userData)
      {
        BoundarySectionNode *self =
            static_cast<BoundarySectionNode *>(userData);
        scene::BoundaryNode *boundary =
            self ? self->enclosingBoundary() : 0;
        loka::core::PushStateTracker *tracker =
            self ? self->tracker()->asPushTracker() : 0;
        if (boundary && tracker)
        {
          boundary->noteInnerTrackerCommit(tracker);
        }
      }

      scene::BoundaryNode *requireEnclosingBoundary() const
      {
        scene::BoundaryNode *boundary = this->enclosingBoundary();
        assert(boundary &&
               "BoundarySection state storage requires an enclosing Boundary");
        return boundary;
      }
    };

    struct BoundarySectionDefinition
        : public scene::NestableNodeDefinition<SectionProps,
                                               BoundarySectionNode,
                                               BoundarySectionDefinition>,
          public scene::TestIdDslMixin<BoundarySectionDefinition>
    {
      typedef scene::NestableNodeDefinition<SectionProps,
                                            BoundarySectionNode,
                                            BoundarySectionDefinition>
          BaseType;
      using BaseType::operator<<;

      BoundarySectionDefinition()
          : BaseType(SectionProps())
      {
      }

      explicit BoundarySectionDefinition(scene::NodeTag key)
          : BaseType(SectionProps(key))
      {
        this->setNodeTag(key);
      }

      explicit BoundarySectionDefinition(const SectionProps &p)
          : BaseType(p)
      {
        this->setNodeTag(p.key());
      }

      BoundarySectionDefinition(const BoundarySectionDefinition &other)
          : BaseType(other.props)
      {
        this->assignFromDefinition(other);
      }

      BoundarySectionDefinition &operator=(const BoundarySectionDefinition &other)
      {
        BaseType::operator=(other);
        return *this;
      }

      virtual scene::NodeDefinitionBase *clone() const
      {
        this->assertValidIdentity();
        BoundarySectionDefinition *copy = new BoundarySectionDefinition(this->props);
        if (!copy)
        {
          return 0;
        }
        if (!copy->assignFromDefinition(*this))
        {
          delete copy;
          return 0;
        }
        return copy;
      }

      virtual scene::Node *create() const
      {
        this->assertValidIdentity();
        return BaseType::create();
      }

      virtual scene::Node *createInPlace(void *mem) const
      {
        this->assertValidIdentity();
        return BaseType::createInPlace(mem);
      }

      virtual bool applyPropsToNode(scene::Node *node) const
      {
        this->assertValidIdentity();
        return BaseType::applyPropsToNode(node);
      }

      virtual bool requiresUniqueSiblingTag() const
      {
        this->assertValidIdentity();
        return true;
      }

    private:
      void assertValidIdentity() const
      {
        assert(this->props.key() != scene::NODE_TAG_NONE &&
               "BoundarySection requires a nonzero value key");
        assert(this->nodeTag() == this->props.key() &&
               "BoundarySection value key is its NodeTag seat identity");
      }

      BoundarySectionDefinition &tag(scene::NodeTag);
    };

    typedef BoundarySectionDefinition BoundarySection;
    typedef BoundarySectionDefinition Section;

  } // namespace app
} // namespace loka

#endif // LOKA_APP2_NODES_NESTABLE_BOUNDARY_SECTION_HPP
