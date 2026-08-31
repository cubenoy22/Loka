#ifndef LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
#define LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/ImageView.hpp"
#include "ImageLoadSession.hpp"
#include "core/State.hpp"
#include "core/String.hpp"
#include "core/resource/Image.hpp"
#include <cassert>

namespace simpleviewer
{
  using loka::app::Button;

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

    MainNode(const MainProps &p)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(p),
          isDialogShown_(),
          chooserResult_(),
          chooserMessage_(),
          image_(),
          imageLoad_()
    {
      this->state(this->isDialogShown_, false);
      this->state(this->chooserResult_, loka::app::FileChooserResult());
      this->state(this->chooserMessage_, loka::core::String::Literal("(none)"));
      this->state(this->image_, loka::core::resource::Image::Empty());
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      this->bindActionForUi(*this->props.openDialogEvent_, &MainNode::openDialog);
      c.declare(
          VStack().alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING)
          << F() << Button("Open...").onClick(this->props.openDialogEvent_) << Text("Loka file:")
          << Text(this->chooserMessage_.state()).attr(TextAttr().wrap(TEXT_WRAP_CHAR).truncation(TEXT_TRUNCATION_NONE))
          << ImageView()
                 .image(this->image_.state())
                 .attr(ImageViewAttr().sizePolicy(IMAGE_VIEW_SIZE_FILL_PARENT).fit(IMAGE_FIT_CONTAIN))
          << (Show(*this->isDialogShown_.state())
              << (PolicyScopeDefinition().destroyOnDetach()
                  << OpenFileDialog().result(this->chooserResult_).testId("SimpleViewerOpenFileDialog"))));
    }

  private:
    friend class ImageLoadSession;

    void openDialog()
    {
      this->imageLoad_.begin(
          *this,
          this->props.platformContext_,
          this->chooserResult_.state(),
          static_cast<loka::core::PushStateTracker *>(this->tracker()));
      this->isDialogShown_.set(true, true);
    }

    bool hasCurrentImage() const
    {
      return this->image_.get().isValid();
    }

    void releaseCurrentImageForLoad()
    {
      const loka::core::resource::Image empty = loka::core::resource::Image::Empty();
      if (this->image_.get() == empty)
      {
        return;
      }
      this->image_.set(empty);
    }

    void commitLoadedImage(const loka::core::resource::Image &image)
    {
      this->image_.set(image, true);
    }

    void closeDialogForChooserResult(const loka::app::FileChooserResult &result)
    {
      if (result.kind != loka::app::FileChooserResult::RESULT_NONE && this->isDialogShown_.get())
      {
        this->isDialogShown_.set(false, true);
      }
    }

    bool isImageLoadDialogShown() const
    {
      return this->isDialogShown_.get();
    }

    void setChooserMessageIfChanged(const loka::core::String &message)
    {
      if (this->chooserMessage_.get().equals(message))
      {
        return;
      }
      this->chooserMessage_.set(message);
    }

    loka::app::scene::NodeState<bool> isDialogShown_;
    loka::app::scene::NodeState<loka::app::FileChooserResult> chooserResult_;
    loka::app::scene::NodeState<loka::core::String> chooserMessage_;
    loka::app::scene::NodeState<loka::core::resource::Image> image_;
    ImageLoadSession imageLoad_;
  };
} // namespace simpleviewer

#include "ImageLoadSessionFlow.hpp"

#endif // LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
