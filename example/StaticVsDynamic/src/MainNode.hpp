#ifndef LOKA_STATIC_VS_DYNAMIC_MAIN_NODE_HPP
#define LOKA_STATIC_VS_DYNAMIC_MAIN_NODE_HPP

#include <cstdio>

#include "app/Button.hpp"
#include "app/Empty.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/node/Conditional.hpp"
#include "app/scene/node/DynamicComposition.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "loka/core/String.hpp"

namespace staticvsdynamic
{
  struct SharedPaneRefs
  {
    SharedPaneRefs()
        : sharedCountText_(0),
          statusText_(0),
          actionEnabled_(0),
          detailsVisible_(0),
          sharedAction_(0)
    {
    }

    loka::core::State<loka::core::String> *sharedCountText_;
    loka::core::State<loka::core::String> *statusText_;
    loka::core::State<bool> *actionEnabled_;
    loka::core::State<bool> *detailsVisible_;
    loka::core::EmitterState *sharedAction_;
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
          detailsDefinition_(loka::app::Text("Details node is visible."))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      ++this->composeCount_;
      std::printf("[StaticVsDynamic] static pane compose #%d\n", this->composeCount_);
      c.declare(
          loka::app::VStack()
          << loka::app::Text("Static Boundary")
          << loka::app::Text(this->props.shared_.sharedCountText_)
          << loka::app::Text(this->props.shared_.statusText_)
          << loka::app::Button("Shared Action", this->props.shared_.sharedAction_).enabled(this->props.shared_.actionEnabled_)
          << c.showIf(*this->props.shared_.detailsVisible_, this->detailsDefinition_));
    }

  private:
    int composeCount_;
    loka::app::TextDefinition detailsDefinition_;
  };

  inline loka::app::scene::BoundaryDefinition<StaticPaneProps, StaticPaneNode> StaticPaneBoundary(const StaticPaneProps &p)
  {
    return loka::app::scene::BoundaryDefinition<StaticPaneProps, StaticPaneNode>(p);
  }

  class DynamicPaneNode : public loka::app::scene::DynamicCompositionBoundaryNodeBase<DynamicPaneProps>
  {
  public:
    DynamicPaneNode(const DynamicPaneProps &p)
        : loka::app::scene::DynamicCompositionBoundaryNodeBase<DynamicPaneProps>(p), composeCount_(0)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      ++this->composeCount_;
      std::printf("[StaticVsDynamic] dynamic pane compose #%d\n", this->composeCount_);
      loka::app::ColumnDefinition column = loka::app::VStack();
      column << loka::app::Text("Dynamic Boundary")
             << loka::app::Text(this->props.shared_.sharedCountText_)
             << loka::app::Text(this->props.shared_.statusText_)
             << loka::app::Button("Shared Action", this->props.shared_.sharedAction_).enabled(this->props.shared_.actionEnabled_);
      if (this->props.shared_.detailsVisible_ && this->props.shared_.detailsVisible_->get())
      {
        column << loka::app::Text("Details node is visible.");
      }
      c.declare(column);
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
  };

  inline loka::app::scene::BoundaryDefinition<DynamicPaneProps, DynamicPaneNode> DynamicPaneBoundary(const DynamicPaneProps &p)
  {
    return loka::app::scene::BoundaryDefinition<DynamicPaneProps, DynamicPaneNode>(p);
  }

  class MainNode;
  typedef loka::app::scene::StaticCompositionPropsFor<MainNode> MainProps;

  class MainNode : public loka::app::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : loka::app::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
          initialized_(false),
          sharedCount_(),
          sharedCountText_(),
          statusText_(),
          detailsVisible_(),
          actionEnabled_(),
          sharedActionEvent_(),
          toggleDetailsEvent_(),
          toggleEnabledEvent_(),
          resetCountEvent_()
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
          .state(this->detailsVisible_, false)
          .state(this->actionEnabled_, true);
      this->bindForUi(this->sharedActionEvent_, this, &MainNode::handleSharedAction);
      this->bindForUi(this->toggleDetailsEvent_, this, &MainNode::toggleDetails);
      this->bindForUi(this->toggleEnabledEvent_, this, &MainNode::toggleEnabled);
      this->bindForUi(this->resetCountEvent_, this, &MainNode::resetCount);
      this->refreshLabels();
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::RowDefinition controls = loka::app::HStack();
      controls << loka::app::Button("Toggle Details", &this->toggleDetailsEvent_)
               << loka::app::Button("Toggle Enabled", &this->toggleEnabledEvent_)
               << loka::app::Button("Reset Count", &this->resetCountEvent_);

      loka::app::RowDefinition panes = loka::app::HStack();
      panes << StaticPaneBoundary(this->makeStaticPaneProps())
            << DynamicPaneBoundary(this->makeDynamicPaneProps());

      c.declare(loka::app::VStack()
                << loka::app::Text("Static vs Dynamic")
                << loka::app::Text("Toggle details to compare showIf against boundary child rebuild.")
                << loka::app::Text("Compose counts are logged to stdout.")
                << controls
                << panes);
    }

  private:
    StaticPaneProps makeStaticPaneProps()
    {
      StaticPaneProps props;
      props.shared_ = this->makeSharedPaneRefs();
      return props;
    }

    DynamicPaneProps makeDynamicPaneProps()
    {
      DynamicPaneProps props;
      props.shared_ = this->makeSharedPaneRefs();
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

    void resetCount()
    {
      this->sharedCount_.set(0, true);
      this->refreshLabels();
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
      this->statusText_.set(loka::core::String::Literal("Details: ") + detailText + loka::core::String::Literal(" / action: ") + enabledText,
                            true);
    }

    bool initialized_;
    loka::app::scene::BoundState<int> sharedCount_;
    loka::app::scene::BoundState<loka::core::String> sharedCountText_;
    loka::app::scene::BoundState<loka::core::String> statusText_;
    loka::app::scene::BoundState<bool> detailsVisible_;
    loka::app::scene::BoundState<bool> actionEnabled_;
    loka::core::EmitterState sharedActionEvent_;
    loka::core::EmitterState toggleDetailsEvent_;
    loka::core::EmitterState toggleEnabledEvent_;
    loka::core::EmitterState resetCountEvent_;
  };
}

#endif // LOKA_STATIC_VS_DYNAMIC_MAIN_NODE_HPP
