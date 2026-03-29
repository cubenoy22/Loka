#include "AttrDslTests.hpp"

#include <cassert>
#include <cstdio>

#include "app/ImageView.hpp"
#include "app/Menu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/scene/PlatformController.hpp"
#include "app/scene/PlatformLayoutHandler.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "loka/core/State.hpp"

namespace
{
  class AttrDslCustomExternalNode : public loka::app::scene::Node
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<AttrDslCustomExternalNode>();
    }
  };

  class AttrDslCustomExternalHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    AttrDslCustomExternalHandler() : ensureCalls_(0) {}

    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<AttrDslCustomExternalNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)controller;
      ++ensureCalls_;
      assert(node != 0);
      assert(node->nodeTypeKey() == this->nodeTypeKey());
      lastState_ = state;
      if (!node->getContext())
      {
        node->setContext(new loka::app::scene::NodeContext(node));
      }
      return node->getContext();
    }

    int ensureCalls_;
    loka::app::scene::LayoutState lastState_;
  };

  class AttrDslDummyRegistrationPlatformController : public loka::app::scene::IPlatformController
  {
  public:
    AttrDslDummyRegistrationPlatformController() : lastRoot_(0) {}

    virtual void onChange(loka::app::scene::Node *rootNode,
                          loka::app::scene::NodeDirtyFlags flags,
                          bool fullRebuild)
    {
      (void)flags;
      (void)fullRebuild;
      this->lastRoot_ = rootNode;
    }

    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    virtual bool registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler)
    {
      return this->registry_.registerHandler(handler);
    }

    loka::app::scene::IPlatformNodeHandler *findHandler(const loka::app::scene::Node *node) const
    {
      return this->registry_.find(node);
    }

    loka::app::scene::PlatformNodeHandlerRegistry registry_;
    loka::app::scene::Node *lastRoot_;
  };

  class AttrDslCustomLayoutLeafNode : public loka::app::scene::Node
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<AttrDslCustomLayoutLeafNode>();
    }
  };

  class AttrDslCustomLayoutNode : public loka::app::scene::NestableNode
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<AttrDslCustomLayoutNode>();
    }
  };

  class AttrDslDummyLayoutTraversal : public loka::app::scene::IPlatformLayoutTraversal
  {
  public:
    AttrDslDummyLayoutTraversal()
        : calls_(0), nextResult_(0), lastChild_(0)
    {
    }

    virtual int layoutChild(loka::app::scene::Node *child, const loka::app::scene::LayoutState &state)
    {
      ++calls_;
      lastChild_ = child;
      lastState_ = state;
      return nextResult_;
    }

    int calls_;
    int nextResult_;
    loka::app::scene::Node *lastChild_;
    loka::app::scene::LayoutState lastState_;
  };

  class AttrDslCustomLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
  {
  public:
    AttrDslCustomLayoutHandler() : layoutCalls_(0), fallbackResult_(0) {}

    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<AttrDslCustomLayoutNode>();
    }

    virtual int layoutNode(loka::app::scene::Node *node,
                           const loka::app::scene::LayoutState &state,
                           loka::app::scene::IPlatformLayoutTraversal *traversal)
    {
      ++layoutCalls_;
      lastState_ = state;
      assert(node != 0);
      assert(node->nodeTypeKey() == this->nodeTypeKey());
      assert(traversal != 0);

      loka::app::scene::INestable *nestable = node->asNestable();
      if (!nestable || !nestable->childrenHead())
      {
        return fallbackResult_;
      }

      loka::app::scene::LayoutState childState = state;
      childState.x += 3;
      childState.y += 5;
      childState.width -= 6;
      if (childState.width < 0)
      {
        childState.width = 0;
      }
      return traversal->layoutChild(nestable->childrenHead(), childState);
    }

    int layoutCalls_;
    int fallbackResult_;
    loka::app::scene::LayoutState lastState_;
  };
}

void testLokaAttrDslV1Core()
{
  printf("\n==== [testLokaAttrDslV1Core] start ====\n");

  // --- v1 attr storage: Text/ImageView should preserve attr on props ---
  {
    loka::core::MutableState<int> dynamicFontSize(22);
    loka::app::TextDefinitionWithAttr text = loka::app::Text("Hello").attr(
        loka::app::TextAttr().fontSize(&dynamicFontSize).weight(loka::app::TEXT_WEIGHT_BOLD));
    assert(text.props.hasAttr_);
    assert(text.props.attr_.fontSizeState_ == &dynamicFontSize);
    assert(text.props.attr_.hasWeightValue_);
    assert(text.props.attr_.weightValue_ == loka::app::TEXT_WEIGHT_BOLD);
    assert(!text.props.attr_.hasWrapValue_);
    assert(!text.props.attr_.hasTruncationValue_);

    loka::app::ImageViewDefinitionWithAttr image = loka::app::ImageView().attr(
        loka::app::ImageViewAttr()
            .sizePolicy(loka::app::IMAGE_VIEW_SIZE_FILL_PARENT)
            .fit(loka::app::IMAGE_FIT_CONTAIN));
    assert(image.props.hasAttr_);
    assert(image.props.attr_.hasFitValue_);
    assert(image.props.attr_.fitValue_ == loka::app::IMAGE_FIT_CONTAIN);
    assert(image.props.attr_.hasSizePolicyValue_);
    assert(image.props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT);
  }

  // --- Text overflow attr storage ---
  {
    loka::app::TextDefinitionWithAttr text = loka::app::Text("Hello").attr(
        loka::app::TextAttr()
            .wrap(loka::app::TEXT_WRAP_WORD)
            .truncation(loka::app::TEXT_TRUNCATION_ELLIPSIS));
    assert(text.props.hasAttr_);
    assert(text.props.attr_.hasWrapValue_);
    assert(text.props.attr_.wrapValue_ == loka::app::TEXT_WRAP_WORD);
    assert(text.props.attr_.hasTruncationValue_);
    assert(text.props.attr_.truncationValue_ == loka::app::TEXT_TRUNCATION_ELLIPSIS);

    loka::app::TextDefinitionWithAttr charWrap = loka::app::Text("Path").attr(
        loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_CHAR));
    assert(charWrap.props.attr_.hasWrapValue_);
    assert(charWrap.props.attr_.wrapValue_ == loka::app::TEXT_WRAP_CHAR);
  }

  // --- Row/Column alignment props storage ---
  {
    loka::app::VStack column = loka::app::VStack().alignHorizontal(loka::app::HORIZONTAL_ALIGNMENT_CENTER);
    assert(column.props.hasHorizontalAlignment_);
    assert(column.props.horizontalAlignment_ == loka::app::HORIZONTAL_ALIGNMENT_CENTER);

    loka::app::HStack row = loka::app::HStack().alignVertical(loka::app::VERTICAL_ALIGNMENT_BOTTOM);
    assert(row.props.hasVerticalAlignment_);
    assert(row.props.verticalAlignment_ == loka::app::VERTICAL_ALIGNMENT_BOTTOM);
  }

  // --- Column remaining-height helper ---
  {
    assert(loka::app::layout::remainingChildHeightForColumn(400, 20, 20) == 400);
    assert(loka::app::layout::remainingChildHeightForColumn(400, 20, 120) == 300);
    assert(loka::app::layout::remainingChildHeightForColumn(400, 20, 999) == 0);
    assert(loka::app::layout::remainingChildHeightForColumn(0, 20, 50) == 0);
  }

  // --- v1 attr copy safety (POD): copy should stay independent ---
  {
    loka::app::TextAttr a;
    loka::app::TextAttr b = a;
    a.fontSize(14);
    assert(!b.hasFontSizeValue_);
    assert(a.hasFontSizeValue_);
    assert(a.fontSizeValue_ == 14);
  }

  // --- MenuItem attr participates in structure equality ---
  {
    loka::core::MutableState<bool> disabledState(true);
    loka::app::MenuItemDefinitionWithAttr left =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(&disabledState));

    loka::app::MenuItemDefinitionWithAttr right =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(&disabledState));
    assert(left.equalsStructure(right));

    loka::app::MenuItemDefinitionWithAttr changed =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(false));
    assert(!left.equalsStructure(changed));
  }

  // --- MenuItem.attr is chain-style: returned definition carries attr ---
  {
    loka::app::MenuItemDefinitionWithAttr item =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(true));
    assert(item.hasAttr_);
    assert(item.attr_.hasDisabledValue_);
    assert(item.attr_.disabledValue_);
  }

  // --- MenuItem disabled attr -> effective enabled projection ---
  {
    loka::app::MenuItemDefinitionWithAttr disabledByValue =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(true));
    assert(!disabledByValue.isEnabledInitial());
    assert(disabledByValue.enabledBindingState() == 0);

    loka::core::MutableState<bool> disabledState(false);
    loka::app::MenuItemDefinitionWithAttr disabledByState =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(&disabledState));
    assert(disabledByState.isEnabledInitial());
    assert(disabledByState.enabledBindingState() == &disabledState);
    assert(disabledByState.enabledBindingInvert());
    disabledState.set(true);
    assert(!disabledByState.isEnabledInitial());

    loka::core::MutableState<bool> enabledState(true);
    loka::app::MenuItemDefinitionWithAttr explicitEnabled =
        loka::app::MenuItem("Open").enabled(&enabledState).attr(loka::app::MenuItemAttr().disabled(true));
    assert(explicitEnabled.enabledBindingState() == &enabledState);
    assert(!explicitEnabled.enabledBindingInvert());
    assert(explicitEnabled.isEnabledInitial());
    enabledState.set(false);
    assert(!explicitEnabled.isEnabledInitial());
  }

  // --- MenuItem visible projection ---
  {
    loka::app::MenuItemDefinitionWithAttr hiddenByValue =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().visible(false));
    assert(!hiddenByValue.isVisibleInitial());

    loka::core::MutableState<bool> visibleState(false);
    loka::app::MenuItemDefinitionWithAttr hiddenByState =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().visible(&visibleState));
    assert(!hiddenByState.isVisibleInitial());
    visibleState.set(true);
    assert(hiddenByState.isVisibleInitial());
  }

  printf("==== [testLokaAttrDslV1Core] end ====\n");
}

void testPlatformNodeHandlerRegistration()
{
  printf("\n==== [testPlatformNodeHandlerRegistration] start ====\n");

  AttrDslCustomExternalNode node;
  AttrDslCustomExternalHandler handler;

  {
    loka::app::scene::PlatformNodeHandlerRegistry registry;
    assert(registry.find(&node) == 0);
    assert(registry.registerHandler(&handler));
    assert(registry.find(&node) == &handler);
  }

  AttrDslDummyRegistrationPlatformController controller;
  loka::app::scene::IPlatformController &platform = controller;
  assert(platform.registerNodeHandler(&handler));
  assert(controller.findHandler(&node) == &handler);

  loka::app::scene::LayoutState state;
  state.x = 4;
  state.y = 8;
  state.width = 16;
  state.height = 32;
  loka::app::scene::IPlatformNodeHandler *resolved = controller.findHandler(&node);
  assert(resolved != 0);
  loka::app::scene::NodeContext *context = resolved->ensureContext(&node, &platform, state);
  assert(context != 0);
  assert(node.getContext() == context);
  assert(handler.ensureCalls_ == 1);
  assert(handler.lastState_.x == 4);
  assert(handler.lastState_.y == 8);
  assert(handler.lastState_.width == 16);
  assert(handler.lastState_.height == 32);

  printf("==== [testPlatformNodeHandlerRegistration] end ====\n");
}

void testPlatformLayoutHandlerRegistration()
{
  printf("\n==== [testPlatformLayoutHandlerRegistration] start ====\n");

  AttrDslCustomLayoutNode node;
  AttrDslCustomLayoutLeafNode *child = new AttrDslCustomLayoutLeafNode();
  node.addChild(child);

  AttrDslCustomLayoutHandler handler;
  AttrDslDummyLayoutTraversal traversal;
  traversal.nextResult_ = 73;

  {
    loka::app::scene::PlatformLayoutHandlerRegistry registry;
    assert(registry.find(&node) == 0);
    assert(registry.registerHandler(&handler));
    assert(registry.find(&node) == &handler);

    loka::app::scene::LayoutState state;
    state.x = 10;
    state.y = 20;
    state.width = 40;
    state.height = 50;

    loka::app::scene::IPlatformLayoutHandler *resolved = registry.find(&node);
    assert(resolved != 0);
    const int resultY = resolved->layoutNode(&node, state, &traversal);
    assert(resultY == 73);
    assert(handler.layoutCalls_ == 1);
    assert(handler.lastState_.x == 10);
    assert(handler.lastState_.y == 20);
    assert(traversal.calls_ == 1);
    assert(traversal.lastChild_ == child);
    assert(traversal.lastState_.x == 13);
    assert(traversal.lastState_.y == 25);
    assert(traversal.lastState_.width == 34);
    assert(traversal.lastState_.height == 50);
  }

  printf("==== [testPlatformLayoutHandlerRegistration] end ====\n");
}
