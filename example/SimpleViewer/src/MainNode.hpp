#ifndef LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
#define LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/ImageView.hpp"
#include "SimpleViewerFlowAdapters.hpp"
#include "core/State.hpp"
#include "core/String.hpp"
#include "core/resource/Image.hpp"
#include <cassert>

namespace simpleviewer
{
  class MainTypeTag
  {
  };

  class MainNode;

  struct MainProps : public loka::app::scene::NodePropsBase<MainProps>
  {
    typedef MainTypeTag TypeTag;
    typedef MainNode NodeType;
    PlatformContext *platformContext_;
    loka::core::EmitterState *openDialogEvent_;
    MainProps()
        : platformContext_(0),
          openDialogEvent_(0)
    {
    }

    MainProps &platformContext(PlatformContext *context)
    {
      this->platformContext_ = context;
      return *this;
    }

    MainProps &openDialogEvent(loka::core::EmitterState *eventState)
    {
      this->openDialogEvent_ = eventState;
      return *this;
    }

    void assertInitialized() const
    {
      assert(this->platformContext_);
      assert(this->openDialogEvent_);
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      if (platformContext_ != other.platformContext_)
        return platformContext_ < other.platformContext_;
      if (openDialogEvent_ != other.openDialogEvent_)
        return openDialogEvent_ < other.openDialogEvent_;
      return false;
    }
  };

  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
  public:
    typedef MainTypeTag TypeTag;
    typedef loka::dsl::FlowChain<loka::app::FileChooserResult, loka::core::resource::Image> ViewerFlowChain;

    MainNode(const MainProps &p)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(p),
          initialized_(false),
          isDialogShown_(),
          chooserResult_(),
          chooserMessage_(),
          image_(),
          flow_()
    {
      this->state(this->isDialogShown_, false);
      this->state(this->chooserResult_, loka::app::FileChooserResult());
      this->state(this->chooserMessage_, loka::core::String::Literal("(none)"));
      this->state(this->image_, loka::core::resource::Image::Empty());
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      this->props.assertInitialized();
      (void)c;
      this->bindActionForUi(*this->props.openDialogEvent_, &MainNode::openDialog);
      this->flow_.set(buildFlow(*this))
          .bindTrigger(this->chooserResult_)
          .withTracker(static_cast<loka::core::PushStateTracker *>(this->tracker()));
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      c.declare(
          VStack().alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING)
          << F() << Button("Open...").onClick(this->props.openDialogEvent_) << Text("Loka file:")
          << Text(this->chooserMessage_.state()).attr(TextAttr().wrap(TEXT_WRAP_CHAR).truncation(TEXT_TRUNCATION_NONE))
          << ImageView()
                 .image(this->image_.state())
                 .attr(ImageViewAttr().sizePolicy(IMAGE_VIEW_SIZE_FILL_PARENT).fit(IMAGE_FIT_CONTAIN))
          << (Show(*this->isDialogShown_.state())
              << (PolicyScope().destroyOnDetach()
                  << OpenFileDialog().result(this->chooserResult_).testId("SimpleViewerOpenFileDialog"))));
    }

  private:
    static bool IsNoFileSelectedError(const loka::dsl::FlowError &error, void *);
    static loka::dsl::FlowHandleResult OnBlobDecodeFailure(const loka::dsl::FlowError &error, void *userData);
    static loka::dsl::FlowHandleResult OnBlobLoadCanceled(const loka::dsl::FlowError &error, void *userData);
    static loka::dsl::FlowHandleResult OnBlobLoadFailure(const loka::dsl::FlowError &error, void *userData);
    static void OnChooserCompletion(const simpleviewer::ChooserContext &context, void *userData);
    static void OnChooserProjection(const simpleviewer::ChooserProjection &projection, void *userData);
    static void OnImageDecoded(const loka::core::resource::Image &image, void *userData);
    static ViewerFlowChain buildFlow(MainNode &self);
    static loka::core::String buildErrorMessage(const loka::dsl::FlowError &error);

    void openDialog()
    {
      this->isDialogShown_.set(true, true);
    }

    void setEmptyImageIfNeeded()
    {
      const loka::core::resource::Image empty = loka::core::resource::Image::Empty();
      if (this->image_.get() == empty)
      {
        return;
      }
      this->image_.set(empty);
    }

    void setChooserMessageIfChanged(const loka::core::String &message)
    {
      if (this->chooserMessage_.get().equals(message))
      {
        return;
      }
      this->chooserMessage_.set(message);
    }

    bool initialized_;
    loka::app::scene::NodeState<bool> isDialogShown_;
    loka::app::scene::NodeState<loka::app::FileChooserResult> chooserResult_;
    loka::app::scene::NodeState<loka::core::String> chooserMessage_;
    loka::app::scene::NodeState<loka::core::resource::Image> image_;
    loka::app::scene::FlowSlot<ViewerFlowChain> flow_;
  };
} // namespace simpleviewer

#include "MainNodeFlow.hpp"

#endif // LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
