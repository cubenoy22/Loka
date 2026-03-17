#ifndef LOKA_STATIC_VS_DYNAMIC_MAIN_NODE_HPP
#define LOKA_STATIC_VS_DYNAMIC_MAIN_NODE_HPP

#include <cassert>
#include <cstdio>
#include <cstring>

#include "app/Button.hpp"
#include "app/Empty.hpp"
#include "app/EditText.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/Window.hpp"
#include "app/scene/BoundState.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/node/Conditional.hpp"
#include "app/scene/node/DynamicComposition.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "loka/core/String.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"

class Window;

namespace staticvsdynamic
{
  class MainBoundary
  {
  public:
    static const char *kInterfaceName() { return "StaticVsDynamicMainBoundary"; }

    static MainBoundary *fromNode(loka::app::scene::Node *node)
    {
      if (!node)
      {
        return 0;
      }
      return static_cast<MainBoundary *>(node->queryInterface(kInterfaceName()));
    }

    virtual ~MainBoundary() {}
    virtual void noteStaticCompose(int count) = 0;
    virtual void noteDynamicCompose(int count) = 0;
    virtual void registerDynamicBoundary(loka::app::scene::BoundaryNode *boundary) = 0;
  };

  class MainNode;

  struct SharedPaneRefs
  {
    SharedPaneRefs()
        : sharedCountText_(0),
          statusText_(0),
          composeCountText_(0),
          actionEnabled_(0),
          detailsVisible_(0),
          sharedAction_(0),
          editText_(0),
          selectedIndex_(0)
    {
    }

    loka::core::State<loka::core::String> *sharedCountText_;
    loka::core::State<loka::core::String> *statusText_;
    loka::core::State<loka::core::String> *composeCountText_;
    loka::core::State<bool> *actionEnabled_;
    loka::core::State<bool> *detailsVisible_;
    loka::core::EmitterState *sharedAction_;
    loka::core::State<loka::core::String> *editText_;
    loka::core::State<int> *selectedIndex_;
  };

  class StaticPaneNode;
  struct StaticPaneTypeTag
  {
  };
  struct StaticPaneProps : public loka::app::scene::NodePropsBase<StaticPaneProps>
  {
    typedef StaticPaneTypeTag TypeTag;
    typedef StaticPaneNode NodeType;

    SharedPaneRefs shared_;

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const StaticPaneProps &other = static_cast<const StaticPaneProps &>(rhs);
      if (shared_.sharedCountText_ != other.shared_.sharedCountText_)
      {
        return shared_.sharedCountText_ < other.shared_.sharedCountText_;
      }
      if (shared_.statusText_ != other.shared_.statusText_)
      {
        return shared_.statusText_ < other.shared_.statusText_;
      }
      if (shared_.actionEnabled_ != other.shared_.actionEnabled_)
      {
        return shared_.actionEnabled_ < other.shared_.actionEnabled_;
      }
      if (shared_.detailsVisible_ != other.shared_.detailsVisible_)
      {
        return shared_.detailsVisible_ < other.shared_.detailsVisible_;
      }
      return shared_.sharedAction_ < other.shared_.sharedAction_;
    }
  };

  class DynamicPaneNode;
  struct DynamicPaneTypeTag
  {
  };
  struct DynamicPaneProps : public loka::app::scene::NodePropsBase<DynamicPaneProps>
  {
    typedef DynamicPaneTypeTag TypeTag;
    typedef DynamicPaneNode NodeType;

    SharedPaneRefs shared_;

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const DynamicPaneProps &other = static_cast<const DynamicPaneProps &>(rhs);
      if (shared_.sharedCountText_ != other.shared_.sharedCountText_)
      {
        return shared_.sharedCountText_ < other.shared_.sharedCountText_;
      }
      if (shared_.statusText_ != other.shared_.statusText_)
      {
        return shared_.statusText_ < other.shared_.statusText_;
      }
      if (shared_.actionEnabled_ != other.shared_.actionEnabled_)
      {
        return shared_.actionEnabled_ < other.shared_.actionEnabled_;
      }
      if (shared_.detailsVisible_ != other.shared_.detailsVisible_)
      {
        return shared_.detailsVisible_ < other.shared_.detailsVisible_;
      }
      return shared_.sharedAction_ < other.shared_.sharedAction_;
    }
  };

  class StaticPaneNode : public loka::app::scene::StaticCompositionBoundaryNodeBase<StaticPaneProps>
  {
  public:
    StaticPaneNode(const StaticPaneProps &p)
        : loka::app::scene::StaticCompositionBoundaryNodeBase<StaticPaneProps>(p),
          composeCount_(0),
          detailsDefinition_(loka::app::Text("Details node is visible.")),
          mainBoundary_(0)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      this->mainBoundary_ = c.findBoundary<MainBoundary>();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      ++this->composeCount_;
      std::printf("[StaticVsDynamic] static pane compose #%d\n", this->composeCount_);
      if (this->mainBoundary_)
      {
        this->mainBoundary_->noteStaticCompose(this->composeCount_);
      }
      c.declare(
          loka::app::VStack()
          << loka::app::Text("Static Boundary")
          << loka::app::Text(this->props.shared_.composeCountText_)
          << loka::app::Text(this->props.shared_.sharedCountText_)
          << loka::app::Text(this->props.shared_.statusText_)
          << loka::app::Button("Shared Action", this->props.shared_.sharedAction_).enabled(this->props.shared_.actionEnabled_).controlTag(600)
          << loka::app::EditText(this->props.shared_.editText_).controlTag(601).testId("StaticPersistEdit")
          << loka::app::PopupMenu(staticItems(), 3).selectedIndex(this->props.shared_.selectedIndex_).controlTag(602).testId("StaticPersistPopup")
          << c.showIf(*this->props.shared_.detailsVisible_, this->detailsDefinition_));
    }

    static const char **staticItems()
    {
      static const char *kItems[] = {"One", "Two", "Three"};
      return kItems;
    }

  private:
    int composeCount_;
    loka::app::TextDefinition detailsDefinition_;
    MainBoundary *mainBoundary_;
  };

  inline loka::app::scene::BoundaryDefinition<StaticPaneProps, StaticPaneNode> StaticPaneBoundary(const StaticPaneProps &p)
  {
    return loka::app::scene::BoundaryDefinition<StaticPaneProps, StaticPaneNode>(p);
  }

  class DynamicPaneNode : public loka::app::scene::DynamicCompositionBoundaryNodeBase<DynamicPaneProps>
  {
  public:
    DynamicPaneNode(const DynamicPaneProps &p)
        : loka::app::scene::DynamicCompositionBoundaryNodeBase<DynamicPaneProps>(p), composeCount_(0), mainBoundary_(0)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      this->mainBoundary_ = c.findBoundary<MainBoundary>();
      if (this->mainBoundary_)
      {
        this->mainBoundary_->registerDynamicBoundary(this);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      ++this->composeCount_;
      std::printf("[StaticVsDynamic] dynamic pane compose #%d\n", this->composeCount_);
      if (this->mainBoundary_)
      {
        this->mainBoundary_->noteDynamicCompose(this->composeCount_);
      }
      loka::app::ColumnDefinition column = loka::app::VStack();
      column << loka::app::Text("Dynamic Boundary")
             << loka::app::Text(this->props.shared_.composeCountText_)
             << loka::app::Text(this->props.shared_.sharedCountText_)
             << loka::app::Text(this->props.shared_.statusText_)
             << loka::app::Button("Shared Action", this->props.shared_.sharedAction_).enabled(this->props.shared_.actionEnabled_).controlTag(700)
             << loka::app::EditText(this->props.shared_.editText_).controlTag(701).testId("DynamicPersistEdit")
             << loka::app::PopupMenu(staticItems(), 3).selectedIndex(this->props.shared_.selectedIndex_).controlTag(702).testId("DynamicPersistPopup");
      if (this->props.shared_.detailsVisible_ && this->props.shared_.detailsVisible_->get())
      {
        column << loka::app::Text("Dynamic rebuild branch").testId("DynamicDetailsText");
      }
      else
      {
        column << loka::app::Button("Dynamic hidden branch", this->props.shared_.sharedAction_).controlTag(703)
                      .enabled(this->props.shared_.actionEnabled_)
                      .testId("DynamicDetailsButton");
      }
      c.declare(column);
    }

    static const char **staticItems()
    {
      static const char *kItems[] = {"One", "Two", "Three"};
      return kItems;
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (this->props.shared_.detailsVisible_)
      {
        registrar.observe(this->props.shared_.detailsVisible_, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }

  private:
    int composeCount_;
    MainBoundary *mainBoundary_;
  };

  inline loka::app::scene::BoundaryDefinition<DynamicPaneProps, DynamicPaneNode> DynamicPaneBoundary(const DynamicPaneProps &p)
  {
    return loka::app::scene::BoundaryDefinition<DynamicPaneProps, DynamicPaneNode>(p);
  }

  typedef loka::app::scene::StaticCompositionPropsFor<MainNode> MainProps;

  class MainNode : public loka::app::scene::StaticCompositionNodeFor<MainNode>, public MainBoundary
  {
  public:
    MainNode(const MainProps &p)
        : loka::app::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
          initialized_(false),
          composeProbeRefreshPending_(false),
          lastStaticComposeCount_(0),
          lastDynamicComposeCount_(0),
          sharedCount_(),
          sharedCountText_(),
          statusText_(),
          staticComposeCountText_(),
          dynamicComposeCountText_(),
          editText_(),
          selectedIndex_(),
          detailsVisible_(),
          dynamicFrozen_(),
          actionEnabled_(),
          sharedActionEvent_(),
          toggleDetailsEvent_(),
          toggleFreezeEvent_(),
          toggleEnabledEvent_(),
          resetCountEvent_(),
          dynamicBoundary_(0)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates()
          .state(this->sharedCount_, 0)
          .state(this->sharedCountText_, loka::core::String::Literal("Shared clicks: 0"))
          .state(this->statusText_, loka::core::String::Literal("Details: hidden / action: enabled"))
          .state(this->staticComposeCountText_, loka::core::String::Literal("Static compose count: 0"))
          .state(this->dynamicComposeCountText_, loka::core::String::Literal("Dynamic compose count: 0"))
          .state(this->editText_, loka::core::String::Literal("Edit me"))
          .state(this->selectedIndex_, 1)
          .state(this->detailsVisible_, false)
          .state(this->dynamicFrozen_, false)
          .state(this->actionEnabled_, true);
      this->bindForUi(this->sharedActionEvent_, this, &MainNode::handleSharedAction);
      this->bindForUi(this->toggleDetailsEvent_, this, &MainNode::toggleDetails);
      this->bindForUi(this->toggleFreezeEvent_, this, &MainNode::toggleFreeze);
      this->bindForUi(this->toggleEnabledEvent_, this, &MainNode::toggleEnabled);
      this->bindForUi(this->resetCountEvent_, this, &MainNode::resetCount);
      this->refreshLabels();
      this->scheduleComposeProbeRefresh();
      this->initialized_ = true;
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *ctx = this->attachedContext();
      return ctx ? ctx->window() : 0;
    }

    virtual void *queryInterface(const char *name)
    {
      if (std::strcmp(name, MainBoundary::kInterfaceName()) == 0)
      {
        return static_cast<MainBoundary *>(this);
      }
      return loka::app::scene::StaticCompositionNodeFor<MainNode>::queryInterface(name);
    }

    virtual void noteStaticCompose(int count)
    {
      this->lastStaticComposeCount_ = count;
      this->scheduleComposeProbeRefresh();
    }

    virtual void noteDynamicCompose(int count)
    {
      this->lastDynamicComposeCount_ = count;
      this->scheduleComposeProbeRefresh();
    }

    virtual void registerDynamicBoundary(loka::app::scene::BoundaryNode *boundary)
    {
      assert((this->dynamicBoundary_ == 0 || this->dynamicBoundary_ == boundary) &&
             "StaticVsDynamic should register exactly one dynamic boundary");
      this->dynamicBoundary_ = boundary;
      if (this->dynamicBoundary_)
      {
        this->dynamicBoundary_->setFrozen(this->dynamicFrozen_.get());
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::RowDefinition controls = loka::app::HStack();
      controls << loka::app::Button("Toggle Details", &this->toggleDetailsEvent_)
                     .controlTag(1001)
               << loka::app::Button(this->dynamicFrozen_.get() ? "Unfreeze Dynamic" : "Freeze Dynamic", &this->toggleFreezeEvent_)
                     .controlTag(1002)
               << loka::app::Button("Toggle Enabled", &this->toggleEnabledEvent_)
                     .controlTag(1003)
               << loka::app::Button("Reset Count", &this->resetCountEvent_)
                     .controlTag(1004);

      loka::app::RowDefinition panes = loka::app::HStack();
      panes << StaticPaneBoundary(this->makeStaticPaneProps())
            << DynamicPaneBoundary(this->makeDynamicPaneProps());

      c.declare(loka::app::VStack()
                << loka::app::Text("Static vs Dynamic")
                << loka::app::Text("Toggle details to compare showIf against boundary child rebuild.")
                << controls
                << panes);
    }

  private:
    StaticPaneProps makeStaticPaneProps()
    {
      StaticPaneProps props;
      props.shared_ = this->makeSharedPaneRefs();
      props.shared_.composeCountText_ = this->staticComposeCountText_.state();
      return props;
    }

    DynamicPaneProps makeDynamicPaneProps()
    {
      DynamicPaneProps props;
      props.shared_ = this->makeSharedPaneRefs();
      props.shared_.composeCountText_ = this->dynamicComposeCountText_.state();
      return props;
    }

    SharedPaneRefs makeSharedPaneRefs()
    {
      SharedPaneRefs refs;
      refs.sharedCountText_ = this->sharedCountText_.state();
      refs.statusText_ = this->statusText_.state();
      refs.actionEnabled_ = this->actionEnabled_.state();
      refs.detailsVisible_ = this->detailsVisible_.state();
      refs.sharedAction_ = &this->sharedActionEvent_;
      refs.editText_ = this->editText_.state();
      refs.selectedIndex_ = this->selectedIndex_.state();
      return refs;
    }

    void handleSharedAction()
    {
      this->sharedCount_.set(this->sharedCount_.get() + 1, true);
      this->refreshLabels();
    }

    void toggleDetails()
    {
      this->detailsVisible_.set(!this->detailsVisible_.get(), true);
      this->refreshLabels();
    }

    void toggleEnabled()
    {
      this->actionEnabled_.set(!this->actionEnabled_.get(), true);
      this->refreshLabels();
    }

    void toggleFreeze()
    {
      bool frozen = !this->dynamicFrozen_.get();
      this->dynamicFrozen_.set(frozen, true);
      if (this->dynamicBoundary_)
      {
        this->dynamicBoundary_->setFrozen(frozen);
      }
      this->refreshLabels();
    }

    void resetCount()
    {
      this->sharedCount_.set(0, true);
      this->refreshLabels();
    }

    static void RefreshComposeProbeThunk(void *userData)
    {
      MainNode *self = static_cast<MainNode *>(userData);
      if (!self)
      {
        return;
      }
      self->composeProbeRefreshPending_ = false;
      self->refreshCountsLabel();
      self->refreshWindowTitle();
    }

    void scheduleComposeProbeRefresh()
    {
      if (this->composeProbeRefreshPending_)
      {
        return;
      }
      loka::core::StateTracker *tracker = this->tracker();
      if (!tracker)
      {
        return;
      }
      if (tracker->phase() == loka::core::TRACKER_IDLE)
      {
        this->refreshCountsLabel();
        this->refreshWindowTitle();
        return;
      }
      this->composeProbeRefreshPending_ = true;
      tracker->defer(&MainNode::RefreshComposeProbeThunk, this);
    }

    void refreshLabels()
    {
      this->sharedCountText_.set(loka::core::String::Literal("Shared clicks: ") + loka::core::String::FromInt(this->sharedCount_.get()), true);
      const loka::core::String detailText = this->detailsVisible_.get()
                                                ? loka::core::String::Literal("shown")
                                                : loka::core::String::Literal("hidden");
      const loka::core::String enabledText = this->actionEnabled_.get()
                                                 ? loka::core::String::Literal("enabled")
                                                 : loka::core::String::Literal("disabled");
      const loka::core::String freezeText = this->dynamicFrozen_.get()
                                                ? loka::core::String::Literal("frozen")
                                                : loka::core::String::Literal("live");
      this->statusText_.set(loka::core::String::Literal("Details: ") + detailText
                                + loka::core::String::Literal(" / action: ") + enabledText
                                + loka::core::String::Literal(" / dynamic: ") + freezeText,
                            true);
    }

    void refreshCountsLabel()
    {
      this->staticComposeCountText_.set(loka::core::String::Literal("Static compose count: ")
                                            + loka::core::String::FromInt(this->lastStaticComposeCount_),
                                        true);
      this->dynamicComposeCountText_.set(loka::core::String::Literal("Dynamic compose count: ")
                                             + loka::core::String::FromInt(this->lastDynamicComposeCount_),
                                         true);
    }

    void refreshWindowTitle()
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      loka::core::StateTrackerGuard guard(window->getTracker());
      window->titleState().set(loka::core::String::Literal("LokaStaticVsDynamic S:")
                                   + loka::core::String::FromInt(this->lastStaticComposeCount_)
                                   + loka::core::String::Literal(" D:")
                                   + loka::core::String::FromInt(this->lastDynamicComposeCount_));
    }

    bool initialized_;
    bool composeProbeRefreshPending_;
    int lastStaticComposeCount_;
    int lastDynamicComposeCount_;
    loka::app::scene::BoundState<int> sharedCount_;
    loka::app::scene::BoundState<loka::core::String> sharedCountText_;
    loka::app::scene::BoundState<loka::core::String> statusText_;
    loka::app::scene::BoundState<loka::core::String> staticComposeCountText_;
    loka::app::scene::BoundState<loka::core::String> dynamicComposeCountText_;
    loka::app::scene::BoundState<loka::core::String> editText_;
    loka::app::scene::BoundState<int> selectedIndex_;
    loka::app::scene::BoundState<bool> detailsVisible_;
    loka::app::scene::BoundState<bool> dynamicFrozen_;
    loka::app::scene::BoundState<bool> actionEnabled_;
    loka::core::EmitterState sharedActionEvent_;
    loka::core::EmitterState toggleDetailsEvent_;
    loka::core::EmitterState toggleFreezeEvent_;
    loka::core::EmitterState toggleEnabledEvent_;
    loka::core::EmitterState resetCountEvent_;
    loka::app::scene::BoundaryNode *dynamicBoundary_;
  };
}

#endif // LOKA_STATIC_VS_DYNAMIC_MAIN_NODE_HPP
