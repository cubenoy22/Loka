#include "StackAxisRetentionTests.hpp"

#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/RecomposingBoundary.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  class StackAxisRetentionBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<StackAxisRetentionBoundaryNode>
      StackAxisRetentionBoundaryProps;

  class StackAxisRetentionBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            StackAxisRetentionBoundaryNode,
            StackAxisRetentionBoundaryProps,
            true>
  {
  public:
    explicit StackAxisRetentionBoundaryNode(
        const StackAxisRetentionBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<
              StackAxisRetentionBoundaryNode,
              StackAxisRetentionBoundaryProps,
              true>(props),
          axis_(loka::app::STACK_AXIS_ROW)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      if (this->axis_ == loka::app::STACK_AXIS_ROW)
      {
        root << (loka::app::HStack() << loka::app::Text("x"));
        composition.declare(root);
        return;
      }
      root << (loka::app::VStack() << loka::app::Text("x"));
      composition.declare(root);
    }

    void setAxis(loka::app::StackAxis axis)
    {
      this->axis_ = axis;
    }

    loka::app::StackNode *stack() const
    {
      loka::app::scene::Node *root = this->compositionRootNode();
      loka::app::scene::INestable *nestable = root ? root->asNestable() : 0;
      loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
      return child ? child->asStackNode() : 0;
    }

  private:
    loka::app::StackAxis axis_;
  };

  struct DefinitionRepointProbeTypeTag
  {
  };

  class DefinitionRepointProbeNode;

  struct DefinitionRepointProbeProps
      : public loka::app::scene::NodePropsBase<DefinitionRepointProbeProps>
  {
    typedef DefinitionRepointProbeTypeTag TypeTag;
    typedef DefinitionRepointProbeNode NodeType;

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      return false;
    }
  };

  class DefinitionRepointProbeNode : public loka::app::scene::NestableNode
  {
  public:
    typedef DefinitionRepointProbeTypeTag TypeTag;

    explicit DefinitionRepointProbeNode(const DefinitionRepointProbeProps &propsValue)
        : props(propsValue), repointCount(0)
    {
    }

    DefinitionRepointProbeProps props;
    unsigned repointCount;
  };

  struct DefinitionRepointProbeDefinition
      : public loka::app::scene::NestableNodeDefinition<
            DefinitionRepointProbeProps,
            DefinitionRepointProbeNode,
            DefinitionRepointProbeDefinition>
  {
    typedef loka::app::scene::NestableNodeDefinition<
        DefinitionRepointProbeProps,
        DefinitionRepointProbeNode,
        DefinitionRepointProbeDefinition> BaseType;
    using BaseType::operator<<;

    DefinitionRepointProbeDefinition()
        : BaseType(DefinitionRepointProbeProps())
    {
    }

    DefinitionRepointProbeDefinition(const DefinitionRepointProbeDefinition &other)
        : BaseType(other)
    {
    }

    virtual bool repointRetainedNodeDefinition(loka::app::scene::Node *node) const
    {
      if (!this->isCompatibleWithNode(node))
      {
        return false;
      }
      ++static_cast<DefinitionRepointProbeNode *>(node)->repointCount;
      return true;
    }
  };

  struct RefusingContainerTypeTag
  {
  };

  class RefusingContainerNode;

  struct RefusingContainerProps
      : public loka::app::scene::NodePropsBase<RefusingContainerProps>
  {
    typedef RefusingContainerTypeTag TypeTag;
    typedef RefusingContainerNode NodeType;

    RefusingContainerProps()
        : revision(0)
    {
    }

    explicit RefusingContainerProps(int revisionValue)
        : revision(revisionValue)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const RefusingContainerProps &other =
          static_cast<const RefusingContainerProps &>(rhs);
      return this->revision < other.revision;
    }

    int revision;
  };

  class RefusingContainerNode : public loka::app::scene::NestableNode
  {
  public:
    typedef RefusingContainerTypeTag TypeTag;

    explicit RefusingContainerNode(const RefusingContainerProps &propsValue)
        : props(propsValue)
    {
    }

    RefusingContainerProps props;
  };

  class RefusingContainerApplyProbe
  {
  public:
    RefusingContainerApplyProbe()
        : state_(STATE_IDLE)
    {
    }

    void reset()
    {
      this->state_ = STATE_IDLE;
    }

    void arm()
    {
      this->state_ = STATE_ARMED;
    }

    bool consume(int revision)
    {
      if (this->state_ != STATE_ARMED || revision != 1)
      {
        return false;
      }
      this->state_ = STATE_REFUSED;
      return true;
    }

    bool wasRefused() const
    {
      return this->state_ == STATE_REFUSED;
    }

  private:
    enum State
    {
      STATE_IDLE,
      STATE_ARMED,
      STATE_REFUSED
    };

    State state_;
  };

  RefusingContainerApplyProbe g_refusingContainerApply;

  struct RefusingContainerDefinition
      : public loka::app::scene::NestableNodeDefinition<
            RefusingContainerProps,
            RefusingContainerNode,
            RefusingContainerDefinition>
  {
    typedef loka::app::scene::NestableNodeDefinition<
        RefusingContainerProps,
        RefusingContainerNode,
        RefusingContainerDefinition> BaseType;
    using BaseType::operator<<;

    explicit RefusingContainerDefinition(
        const RefusingContainerProps &propsValue = RefusingContainerProps())
        : BaseType(propsValue)
    {
    }

    virtual bool applyPropsToNode(loka::app::scene::Node *node) const
    {
      if (g_refusingContainerApply.consume(this->props.revision))
      {
        return false;
      }
      return BaseType::applyPropsToNode(node);
    }
  };

  enum NestedRetentionShape
  {
    NESTED_RETENTION_DEPTH_TWO,
    NESTED_RETENTION_DEPTH_THREE,
    NESTED_RETENTION_TAGGED_PAIR,
    NESTED_RETENTION_CHANGED_PARENT_AND_CHILD,
    NESTED_RETENTION_STRUCTURAL_CHILDREN,
    NESTED_RETENTION_MISPLACED_POLICY_SCOPE,
    NESTED_RETENTION_REFUSING_CONTAINER,
    NESTED_RETENTION_REPOINT_PROBE,
    NESTED_RETENTION_DEEP_EQUIVALENT_ANONYMOUS,
    NESTED_RETENTION_DEEP_DIFFERENT_ANONYMOUS
  };

  template <bool UseRetainFastPaths, NestedRetentionShape Shape>
  class NestedRetentionBoundaryNode;

  template <bool UseRetainFastPaths, NestedRetentionShape Shape>
  class NestedRetentionBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            NestedRetentionBoundaryNode<UseRetainFastPaths, Shape>,
            loka::app::scene::BoundaryPropsFor<
                NestedRetentionBoundaryNode<UseRetainFastPaths, Shape> >,
            UseRetainFastPaths>
  {
  public:
    typedef SceneTestSupport::RecomposingBoundaryNode<
        NestedRetentionBoundaryNode<UseRetainFastPaths, Shape>,
        loka::app::scene::BoundaryPropsFor<
            NestedRetentionBoundaryNode<UseRetainFastPaths, Shape> >,
        UseRetainFastPaths> BaseType;
    typedef loka::app::scene::BoundaryPropsFor<
        NestedRetentionBoundaryNode<UseRetainFastPaths, Shape> > PropsType;

    explicit NestedRetentionBoundaryNode(
        const PropsType &props)
        : BaseType(props), changed_(false)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      if (Shape == NESTED_RETENTION_DEPTH_TWO)
      {
        loka::app::Fragment wrapper;
        wrapper << loka::app::Stack(
            this->changed_ ? loka::app::STACK_AXIS_COLUMN : loka::app::STACK_AXIS_ROW);
        root << wrapper;
      }
      else if (Shape == NESTED_RETENTION_DEPTH_THREE)
      {
        loka::app::Fragment outerWrapper;
        loka::app::Fragment innerWrapper;
        innerWrapper << loka::app::Stack(
            this->changed_ ? loka::app::STACK_AXIS_COLUMN : loka::app::STACK_AXIS_ROW);
        outerWrapper << innerWrapper;
        root << outerWrapper;
      }
      else if (Shape == NESTED_RETENTION_TAGGED_PAIR)
      {
        loka::app::Fragment wrapper;
        loka::app::Stack first(
            this->changed_ ? loka::app::STACK_AXIS_COLUMN : loka::app::STACK_AXIS_ROW);
        loka::app::Stack second(loka::app::STACK_AXIS_ROW);
        first.tag(5571);
        second.tag(5572);
        wrapper << first << second;
        root << wrapper;
      }
      else if (Shape == NESTED_RETENTION_CHANGED_PARENT_AND_CHILD)
      {
        const loka::app::StackAxis axis =
            this->changed_ ? loka::app::STACK_AXIS_COLUMN : loka::app::STACK_AXIS_ROW;
        loka::app::Stack parent(axis);
        parent << loka::app::Stack(axis);
        root << parent;
      }
      else if (Shape == NESTED_RETENTION_STRUCTURAL_CHILDREN)
      {
        loka::app::Fragment wrapper;
        loka::app::Stack retained(
            this->changed_ ? loka::app::STACK_AXIS_COLUMN : loka::app::STACK_AXIS_ROW);
        retained.tag(5574);
        if (this->changed_)
        {
          loka::app::Text replacement("replacement");
          replacement.tag(5573);
          wrapper << retained << replacement;
        }
        else
        {
          loka::app::Fragment replaced;
          replaced.tag(5573);
          wrapper << replaced << retained;
        }
        root << wrapper;
      }
      else if (Shape == NESTED_RETENTION_MISPLACED_POLICY_SCOPE)
      {
        loka::app::Fragment wrapper;
        loka::app::PolicyScopeDefinition misplacedScope;
        if (this->changed_)
        {
          loka::app::EditTextDefinition edit;
          misplacedScope.destroyOnDetach() << edit;
        }
        else
        {
          loka::app::ButtonDefinition button("button");
          misplacedScope.destroyOnDetach() << button;
        }
        wrapper << misplacedScope;
        root << wrapper;
      }
      else if (Shape == NESTED_RETENTION_REFUSING_CONTAINER)
      {
        RefusingContainerDefinition wrapper(
            RefusingContainerProps(this->changed_ ? 1 : 0));
        wrapper << loka::app::Stack(
            this->changed_ ? loka::app::STACK_AXIS_COLUMN : loka::app::STACK_AXIS_ROW);
        root << wrapper;
      }
      else if (Shape == NESTED_RETENTION_DEEP_EQUIVALENT_ANONYMOUS ||
               Shape == NESTED_RETENTION_DEEP_DIFFERENT_ANONYMOUS)
      {
        loka::app::Fragment wrapper;
        loka::app::Stack outer(
            this->changed_ ? loka::app::STACK_AXIS_COLUMN : loka::app::STACK_AXIS_ROW);
        loka::app::Stack firstPanel(loka::app::STACK_AXIS_COLUMN);
        loka::app::Stack secondPanel(loka::app::STACK_AXIS_COLUMN);
        wrapper.tag(5575);
        if (Shape == NESTED_RETENTION_DEEP_DIFFERENT_ANONYMOUS)
        {
          firstPanel << loka::app::Text("first");
          if (this->changed_)
          {
            firstPanel << loka::app::EditTextDefinition();
          }
          else
          {
            firstPanel << loka::app::ButtonDefinition("old");
          }
          firstPanel << loka::app::Text("third");
        }
        else
        {
          firstPanel << loka::app::Text("first")
                     << loka::app::Text("second")
                     << loka::app::Text("third");
        }
        secondPanel << loka::app::Text("fourth")
                    << loka::app::Text("fifth")
                    << loka::app::Text("sixth");
        outer << firstPanel << secondPanel;
        wrapper << outer;
        root << wrapper;
      }
      else
      {
        loka::app::Fragment wrapper;
        DefinitionRepointProbeDefinition probe;
        probe << loka::app::Text("probe");
        wrapper << probe;
        root << wrapper;
      }
      composition.declare(root);
    }

    void setChanged()
    {
      this->changed_ = true;
    }

    loka::app::scene::Node *nodeAt(unsigned first,
                                   unsigned second = 999,
                                   unsigned third = 999) const
    {
      loka::app::scene::Node *node = this->compositionRootNode();
      const unsigned slots[3] = {first, second, third};
      for (unsigned depth = 0; depth < 3 && slots[depth] != 999; ++depth)
      {
        loka::app::scene::INestable *nestable = node ? node->asNestable() : 0;
        node = nestable ? nestable->childrenHead() : 0;
        for (unsigned slot = 0; node && slot < slots[depth]; ++slot)
        {
          node = node->nextInComposition;
        }
      }
      return node;
    }

  private:
    bool changed_;
  };

  template <bool UseRetainFastPaths, NestedRetentionShape Shape>
  NestedRetentionBoundaryNode<UseRetainFastPaths, Shape> *mountNestedRetentionBoundary(
      NullScenePlatformController &platform,
      loka::app::scene::Scene &scene)
  {
    scene.mount(&platform);
    scene.updateAttached(true);
    NestedRetentionBoundaryNode<UseRetainFastPaths, Shape> *boundary =
        static_cast<NestedRetentionBoundaryNode<UseRetainFastPaths, Shape> *>(
            loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    LOKA_VERIFY(boundary != 0);
    return boundary;
  }

  loka::app::scene::Node *nestedChildAt(loka::app::scene::Node *parent,
                                        unsigned index)
  {
    loka::app::scene::INestable *nestable = parent ? parent->asNestable() : 0;
    loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
    while (child && index > 0)
    {
      child = child->nextInComposition;
      --index;
    }
    return child;
  }

  template <bool UseRetainFastPaths, NestedRetentionShape Shape>
  void flushNestedRetentionChange(
      NestedRetentionBoundaryNode<UseRetainFastPaths, Shape> *boundary,
      loka::app::scene::Scene &scene)
  {
    boundary->setChanged();
    scene.requestInvalidate(static_cast<loka::app::scene::NodeDirtyFlags>(
        loka::app::scene::NODE_DIRTY_CHILD |
        loka::app::scene::NODE_DIRTY_LAYOUT));
    LOKA_VERIFY(scene.flushInvalidation());
  }

  template <bool UseRetainFastPaths>
  void runDepthTwoNestedRetention()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths, NESTED_RETENTION_DEPTH_TWO> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths, NESTED_RETENTION_DEPTH_TWO>(platform, scene);
    loka::app::scene::Node *wrapper = boundary->nodeAt(0);
    loka::app::StackNode *stack = boundary->nodeAt(0, 0)->asStackNode();
    LOKA_VERIFY(wrapper != 0);
    LOKA_VERIFY(stack != 0);
    LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_ROW);

    flushNestedRetentionChange(boundary, scene);

    LOKA_VERIFY(boundary->nodeAt(0) == wrapper);
    LOKA_VERIFY(boundary->nodeAt(0, 0) == stack);
    LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    scene.unmount();
  }

  template <bool UseRetainFastPaths>
  void runDepthThreeNestedRetention()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths, NESTED_RETENTION_DEPTH_THREE> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths, NESTED_RETENTION_DEPTH_THREE>(platform, scene);
    loka::app::scene::Node *outer = boundary->nodeAt(0);
    loka::app::scene::Node *inner = boundary->nodeAt(0, 0);
    loka::app::StackNode *stack = boundary->nodeAt(0, 0, 0)->asStackNode();
    LOKA_VERIFY(outer != 0);
    LOKA_VERIFY(inner != 0);
    LOKA_VERIFY(stack != 0);

    flushNestedRetentionChange(boundary, scene);

    LOKA_VERIFY(boundary->nodeAt(0) == outer);
    LOKA_VERIFY(boundary->nodeAt(0, 0) == inner);
    LOKA_VERIFY(boundary->nodeAt(0, 0, 0) == stack);
    LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    scene.unmount();
  }

  template <bool UseRetainFastPaths>
  void runTaggedNestedRetention()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths, NESTED_RETENTION_TAGGED_PAIR> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths, NESTED_RETENTION_TAGGED_PAIR>(platform, scene);
    loka::app::scene::Node *wrapper = boundary->nodeAt(0);
    loka::app::StackNode *first = boundary->nodeAt(0, 0)->asStackNode();
    loka::app::StackNode *second = boundary->nodeAt(0, 1)->asStackNode();
    LOKA_VERIFY(wrapper != 0);
    LOKA_VERIFY(first != 0);
    LOKA_VERIFY(second != 0);

    flushNestedRetentionChange(boundary, scene);

    LOKA_VERIFY(boundary->nodeAt(0) == wrapper);
    LOKA_VERIFY(boundary->nodeAt(0, 0) == first);
    LOKA_VERIFY(boundary->nodeAt(0, 1) == second);
    LOKA_VERIFY(first->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    LOKA_VERIFY(second->props.axis_ == loka::app::STACK_AXIS_ROW);
    scene.unmount();
  }

  template <bool UseRetainFastPaths>
  void runChangedParentAndChildRetention()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths, NESTED_RETENTION_CHANGED_PARENT_AND_CHILD> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths, NESTED_RETENTION_CHANGED_PARENT_AND_CHILD>(platform, scene);
    loka::app::StackNode *parent = boundary->nodeAt(0)->asStackNode();
    loka::app::StackNode *child = boundary->nodeAt(0, 0)->asStackNode();
    LOKA_VERIFY(parent != 0);
    LOKA_VERIFY(child != 0);

    flushNestedRetentionChange(boundary, scene);

    LOKA_VERIFY(boundary->nodeAt(0) == parent);
    LOKA_VERIFY(boundary->nodeAt(0, 0) == child);
    LOKA_VERIFY(parent->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    LOKA_VERIFY(child->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    scene.unmount();
  }

  template <bool UseRetainFastPaths>
  void runNestedDefinitionRepoint()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths, NESTED_RETENTION_REPOINT_PROBE> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths, NESTED_RETENTION_REPOINT_PROBE>(platform, scene);
    DefinitionRepointProbeNode *probe =
        static_cast<DefinitionRepointProbeNode *>(boundary->nodeAt(0, 0));
    LOKA_VERIFY(probe != 0);
    LOKA_VERIFY(probe->repointCount == 0);

    flushNestedRetentionChange(boundary, scene);

    LOKA_VERIFY(boundary->nodeAt(0, 0) == probe);
    LOKA_VERIFY(probe->repointCount == 1);
    scene.unmount();
  }

  template <bool UseRetainFastPaths>
  void runStructuralNestedRetention()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths, NESTED_RETENTION_STRUCTURAL_CHILDREN> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths, NESTED_RETENTION_STRUCTURAL_CHILDREN>(platform, scene);
    loka::app::scene::Node *wrapper = boundary->nodeAt(0);
    loka::app::scene::Node *replaced = boundary->nodeAt(0, 0);
    loka::app::StackNode *retained = boundary->nodeAt(0, 1)->asStackNode();
    LOKA_VERIFY(wrapper != 0);
    LOKA_VERIFY(replaced != 0);
    LOKA_VERIFY(retained != 0);
    LOKA_VERIFY(retained->props.axis_ == loka::app::STACK_AXIS_ROW);

    flushNestedRetentionChange(boundary, scene);

    loka::app::scene::Node *first = boundary->nodeAt(0, 0);
    loka::app::scene::Node *second = boundary->nodeAt(0, 1);
    LOKA_VERIFY(boundary->nodeAt(0) == wrapper);
    LOKA_VERIFY(first == retained);
    LOKA_VERIFY(first != replaced);
    LOKA_VERIFY(second != 0);
    LOKA_VERIFY(boundary->nodeAt(0, 2) == 0);
    LOKA_VERIFY(second != replaced);
    LOKA_VERIFY(second != retained);
    LOKA_VERIFY(second->kind() == loka::app::scene::NODE_KIND_TEXT);
    LOKA_VERIFY(first->nodeTag() == 5574);
    LOKA_VERIFY(second->nodeTag() == 5573);
    LOKA_VERIFY(retained->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    scene.unmount();
  }

  template <bool UseRetainFastPaths>
  void runNestedMisplacedPolicyScopeReconciliation()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths, NESTED_RETENTION_MISPLACED_POLICY_SCOPE> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths, NESTED_RETENTION_MISPLACED_POLICY_SCOPE>(platform, scene);
    loka::app::scene::Node *wrapper = boundary->nodeAt(0);
    loka::app::scene::Node *scopeContent = boundary->nodeAt(0, 0);
    loka::app::scene::Node *oldControl = boundary->nodeAt(0, 0, 0);
    LOKA_VERIFY(wrapper != 0);
    LOKA_VERIFY(scopeContent != 0);
    LOKA_VERIFY(oldControl != 0);
    LOKA_VERIFY(oldControl->kind() == loka::app::scene::NODE_KIND_BUTTON);
    LOKA_VERIFY(platform.findLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_BUTTON) != 0);
    LOKA_VERIFY(platform.findLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT) == 0);

    flushNestedRetentionChange(boundary, scene);
    platform.drainNativeRetirements();

    loka::app::scene::Node *newControl = boundary->nodeAt(0, 0, 0);
    LOKA_VERIFY(boundary->nodeAt(0) == wrapper);
    LOKA_VERIFY(boundary->nodeAt(0, 0) == scopeContent);
    LOKA_VERIFY(newControl != 0);
    LOKA_VERIFY(newControl != oldControl);
    LOKA_VERIFY(newControl->kind() == loka::app::scene::NODE_KIND_EDIT_TEXT);
    LOKA_VERIFY(platform.findLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_BUTTON) == 0);
    LOKA_VERIFY(platform.findLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT) != 0);
    scene.unmount();
  }

  template <bool UseRetainFastPaths>
  void runNestedApplyRefusalFallsThroughToCurrentTree()
  {
    g_refusingContainerApply.reset();
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths, NESTED_RETENTION_REFUSING_CONTAINER> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths, NESTED_RETENTION_REFUSING_CONTAINER>(platform, scene);
    RefusingContainerNode *wrapper =
        static_cast<RefusingContainerNode *>(boundary->nodeAt(0));
    loka::app::StackNode *stack = boundary->nodeAt(0, 0)->asStackNode();
    LOKA_VERIFY(wrapper != 0);
    LOKA_VERIFY(stack != 0);
    LOKA_VERIFY(wrapper->props.revision == 0);
    LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_ROW);

    g_refusingContainerApply.arm();
    flushNestedRetentionChange(boundary, scene);

    LOKA_VERIFY(g_refusingContainerApply.wasRefused());
    LOKA_VERIFY(boundary->nodeAt(0) == wrapper);
    LOKA_VERIFY(boundary->nodeAt(0, 0) == stack);
    LOKA_VERIFY(wrapper->props.revision == 1);
    LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    scene.unmount();
    g_refusingContainerApply.reset();
  }

  template <bool UseRetainFastPaths>
  void runDeepEquivalentAnonymousRetention()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths,
        NESTED_RETENTION_DEEP_EQUIVALENT_ANONYMOUS> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths,
        NESTED_RETENTION_DEEP_EQUIVALENT_ANONYMOUS>(platform, scene);
    loka::app::scene::Node *wrapper = boundary->nodeAt(0);
    loka::app::StackNode *outer = boundary->nodeAt(0, 0)->asStackNode();
    loka::app::scene::Node *panels[2] = {
        boundary->nodeAt(0, 0, 0), boundary->nodeAt(0, 0, 1)};
    loka::app::scene::Node *leaves[2][3];
    LOKA_VERIFY(wrapper != 0);
    LOKA_VERIFY(wrapper->nodeTag() == 5575);
    LOKA_VERIFY(outer != 0);
    LOKA_VERIFY(outer->props.axis_ == loka::app::STACK_AXIS_ROW);
    for (unsigned panel = 0; panel < 2; ++panel)
    {
      LOKA_VERIFY(panels[panel] != 0);
      for (unsigned leaf = 0; leaf < 3; ++leaf)
      {
        leaves[panel][leaf] = nestedChildAt(panels[panel], leaf);
        LOKA_VERIFY(leaves[panel][leaf] != 0);
      }
      LOKA_VERIFY(nestedChildAt(panels[panel], 3) == 0);
    }

    flushNestedRetentionChange(boundary, scene);

    LOKA_VERIFY(boundary->nodeAt(0) == wrapper);
    LOKA_VERIFY(boundary->nodeAt(0, 0) == outer);
    LOKA_VERIFY(outer->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    for (unsigned panel = 0; panel < 2; ++panel)
    {
      LOKA_VERIFY(boundary->nodeAt(0, 0, panel) == panels[panel]);
      for (unsigned leaf = 0; leaf < 3; ++leaf)
      {
        LOKA_VERIFY(nestedChildAt(panels[panel], leaf) == leaves[panel][leaf]);
      }
    }
    scene.unmount();
  }

  template <bool UseRetainFastPaths>
  void runDeepDifferentAnonymousLocalRebuild()
  {
    NullScenePlatformController platform;
    typedef NestedRetentionBoundaryNode<
        UseRetainFastPaths,
        NESTED_RETENTION_DEEP_DIFFERENT_ANONYMOUS> BoundaryT;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<BoundaryT>()));
    BoundaryT *boundary = mountNestedRetentionBoundary<
        UseRetainFastPaths,
        NESTED_RETENTION_DEEP_DIFFERENT_ANONYMOUS>(platform, scene);
    loka::app::scene::Node *wrapper = boundary->nodeAt(0);
    loka::app::StackNode *outer = boundary->nodeAt(0, 0)->asStackNode();
    loka::app::scene::Node *oldPanels[2] = {
        boundary->nodeAt(0, 0, 0), boundary->nodeAt(0, 0, 1)};
    loka::app::scene::Node *oldFirstPanelChildren[3];
    LOKA_VERIFY(wrapper != 0);
    LOKA_VERIFY(outer != 0);
    LOKA_VERIFY(oldPanels[0] != 0);
    LOKA_VERIFY(oldPanels[1] != 0);
    for (unsigned child = 0; child < 3; ++child)
    {
      oldFirstPanelChildren[child] = nestedChildAt(oldPanels[0], child);
      LOKA_VERIFY(oldFirstPanelChildren[child] != 0);
    }
    LOKA_VERIFY(oldFirstPanelChildren[1]->kind() ==
                loka::app::scene::NODE_KIND_BUTTON);
    LOKA_VERIFY(platform.findLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_BUTTON) != 0);
    LOKA_VERIFY(platform.findLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT) == 0);

    flushNestedRetentionChange(boundary, scene);
    platform.drainNativeRetirements();

    loka::app::scene::Node *newPanels[2] = {
        boundary->nodeAt(0, 0, 0), boundary->nodeAt(0, 0, 1)};
    LOKA_VERIFY(boundary->nodeAt(0) == wrapper);
    LOKA_VERIFY(boundary->nodeAt(0, 0) == outer);
    LOKA_VERIFY(outer->props.axis_ == loka::app::STACK_AXIS_COLUMN);
    LOKA_VERIFY(newPanels[0] != 0);
    LOKA_VERIFY(newPanels[1] != 0);
    LOKA_VERIFY(newPanels[0] != oldPanels[0]);
    LOKA_VERIFY(newPanels[0] != oldPanels[1]);
    LOKA_VERIFY(newPanels[1] != oldPanels[0]);
    LOKA_VERIFY(newPanels[1] != oldPanels[1]);
    LOKA_VERIFY(boundary->nodeAt(0, 0, 2) == 0);
    loka::app::scene::Node *newFirstPanelChildren[3] = {
        nestedChildAt(newPanels[0], 0),
        nestedChildAt(newPanels[0], 1),
        nestedChildAt(newPanels[0], 2)};
    LOKA_VERIFY(newFirstPanelChildren[0] != 0);
    LOKA_VERIFY(newFirstPanelChildren[1] != 0);
    LOKA_VERIFY(newFirstPanelChildren[2] != 0);
    LOKA_VERIFY(newFirstPanelChildren[0]->kind() ==
                loka::app::scene::NODE_KIND_TEXT);
    LOKA_VERIFY(newFirstPanelChildren[1]->kind() ==
                loka::app::scene::NODE_KIND_EDIT_TEXT);
    LOKA_VERIFY(newFirstPanelChildren[2]->kind() ==
                loka::app::scene::NODE_KIND_TEXT);
    LOKA_VERIFY(nestedChildAt(newPanels[0], 3) == 0);
    LOKA_VERIFY(platform.findLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_BUTTON) == 0);
    LOKA_VERIFY(platform.findLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT) != 0);
    scene.unmount();
  }
} // namespace

void testStackAxisFlipRetainsContainerAndChildAndRelayouts()
{
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<StackAxisRetentionBoundaryNode>()));
  scene.mount(&platform);
  scene.updateAttached(true);

  StackAxisRetentionBoundaryNode *boundary =
      static_cast<StackAxisRetentionBoundaryNode *>(
          loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(boundary != 0);
  loka::app::StackNode *stack = boundary->stack();
  LOKA_VERIFY(stack != 0);
  LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_ROW);
  loka::app::scene::Node *text = stack->childrenHead();
  LOKA_VERIFY(text != 0);
  LOKA_VERIFY(text->kind() == loka::app::scene::NODE_KIND_TEXT);
  const unsigned long layoutCount = platform.onChangeCallCount();

  boundary->setAxis(loka::app::STACK_AXIS_COLUMN);
  scene.requestInvalidate(static_cast<loka::app::scene::NodeDirtyFlags>(
      loka::app::scene::NODE_DIRTY_CHILD |
      loka::app::scene::NODE_DIRTY_LAYOUT));
  LOKA_VERIFY(scene.flushInvalidation());

  LOKA_VERIFY(boundary->stack() == stack);
  LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_COLUMN);
  LOKA_VERIFY(stack->childrenHead() == text);
  LOKA_VERIFY(platform.onChangeCallCount() == layoutCount + 1);
  LOKA_VERIFY((platform.lastOnChangeFlags() &
               loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

  scene.unmount();
}

void testNestedRetainedAnonymousChildAppliesChangedPropsInBothModes()
{
  // false exercises snapshot apply; true exercises retain-fast-path apply.
  runDepthTwoNestedRetention<false>();
  runDepthTwoNestedRetention<true>();
}

void testNestedRetainedDepthThreeAppliesChangedPropsInBothModes()
{
  runDepthThreeNestedRetention<false>();
  runDepthThreeNestedRetention<true>();
}

void testNestedRetainedTaggedChildrenApplyChangedPropsInBothModes()
{
  runTaggedNestedRetention<false>();
  runTaggedNestedRetention<true>();
}

void testNestedChangedParentAndChildPropsApplyInBothModes()
{
  runChangedParentAndChildRetention<false>();
  runChangedParentAndChildRetention<true>();
}

void testNestedEquivalentDefinitionIsRepointedInBothModes()
{
  runNestedDefinitionRepoint<false>();
  runNestedDefinitionRepoint<true>();
}

void testNestedRetainedStructuralChildrenApplyInBothModes()
{
  runStructuralNestedRetention<false>();
  runStructuralNestedRetention<true>();
}

void testNestedMisplacedPolicyScopeReconcilesInBothModes()
{
  runNestedMisplacedPolicyScopeReconciliation<false>();
  runNestedMisplacedPolicyScopeReconciliation<true>();
}

void testNestedApplyRefusalFallsThroughToCurrentTreeInBothModes()
{
  runNestedApplyRefusalFallsThroughToCurrentTree<false>();
  runNestedApplyRefusalFallsThroughToCurrentTree<true>();
}

void testDeepEquivalentAnonymousSubtreesRetainNodesInBothModes()
{
  runDeepEquivalentAnonymousRetention<false>();
  runDeepEquivalentAnonymousRetention<true>();
}

void testDeepDifferentAnonymousSubtreeRebuildsLocallyInBothModes()
{
  runDeepDifferentAnonymousLocalRebuild<false>();
  runDeepDifferentAnonymousLocalRebuild<true>();
}

void testControlPropsEquivalenceDistinguishesOwnedValuesAndBorrowedSources()
{
  loka::app::ButtonDefinition ownedButtonX("X");
  loka::app::ButtonDefinition anotherOwnedButtonX("X");
  loka::app::ButtonDefinition ownedButtonY("Y");
  loka::core::State<loka::core::String> buttonStateA(
      loka::core::String::Literal("X"));
  loka::core::State<loka::core::String> buttonStateB(
      loka::core::String::Literal("X"));
  loka::app::ButtonDefinition borrowedButtonA(&buttonStateA);
  loka::app::ButtonDefinition anotherBorrowedButtonA(&buttonStateA);
  loka::app::ButtonDefinition borrowedButtonB(&buttonStateB);

  LOKA_VERIFY(ownedButtonX.hasEquivalentProps(anotherOwnedButtonX));
  LOKA_VERIFY(!ownedButtonX.hasEquivalentProps(ownedButtonY));
  LOKA_VERIFY(!ownedButtonX.hasEquivalentProps(borrowedButtonA));
  LOKA_VERIFY(!borrowedButtonA.hasEquivalentProps(ownedButtonX));
  LOKA_VERIFY(borrowedButtonA.hasEquivalentProps(anotherBorrowedButtonA));
  LOKA_VERIFY(!borrowedButtonA.hasEquivalentProps(borrowedButtonB));

  loka::app::CellDefinition ownedCellX("X");
  loka::app::CellDefinition anotherOwnedCellX("X");
  loka::app::CellDefinition ownedCellY("Y");
  loka::app::CellDefinition borrowedCellA(&buttonStateA);
  loka::app::CellDefinition anotherBorrowedCellA(&buttonStateA);
  loka::app::CellDefinition borrowedCellB(&buttonStateB);

  LOKA_VERIFY(ownedCellX.hasEquivalentProps(anotherOwnedCellX));
  LOKA_VERIFY(!ownedCellX.hasEquivalentProps(ownedCellY));
  LOKA_VERIFY(!ownedCellX.hasEquivalentProps(borrowedCellA));
  LOKA_VERIFY(!borrowedCellA.hasEquivalentProps(ownedCellX));
  LOKA_VERIFY(borrowedCellA.hasEquivalentProps(anotherBorrowedCellA));
  LOKA_VERIFY(!borrowedCellA.hasEquivalentProps(borrowedCellB));

  loka::app::TextDefinition ownedTextX("X");
  loka::app::TextDefinition anotherOwnedTextX("X");
  loka::app::TextDefinition ownedTextY("Y");
  loka::app::TextDefinition borrowedTextA(&buttonStateA);
  loka::app::TextDefinition anotherBorrowedTextA(&buttonStateA);
  loka::app::TextDefinition borrowedTextB(&buttonStateB);

  LOKA_VERIFY(ownedTextX.hasEquivalentProps(anotherOwnedTextX));
  LOKA_VERIFY(!ownedTextX.hasEquivalentProps(ownedTextY));
  LOKA_VERIFY(!ownedTextX.hasEquivalentProps(borrowedTextA));
  LOKA_VERIFY(!borrowedTextA.hasEquivalentProps(ownedTextX));
  LOKA_VERIFY(borrowedTextA.hasEquivalentProps(anotherBorrowedTextA));
  LOKA_VERIFY(!borrowedTextA.hasEquivalentProps(borrowedTextB));

  loka::core::MutableState<loka::core::String> editStateA(
      loka::core::String::Literal("X"));
  loka::core::MutableState<loka::core::String> editStateB(
      loka::core::String::Literal("X"));
  loka::app::EditTextDefinition emptyEditA;
  loka::app::EditTextDefinition emptyEditB;
  loka::app::EditTextDefinition borrowedEditA(&editStateA);
  loka::app::EditTextDefinition anotherBorrowedEditA(&editStateA);
  loka::app::EditTextDefinition borrowedEditB(&editStateB);

  // EditText is necessarily borrowed: #507 forbids definition-owned
  // two-way bindings because user edits must outlive a definition generation.
  LOKA_VERIFY(emptyEditA.hasEquivalentProps(emptyEditB));
  LOKA_VERIFY(borrowedEditA.hasEquivalentProps(anotherBorrowedEditA));
  LOKA_VERIFY(!borrowedEditA.hasEquivalentProps(borrowedEditB));
}
