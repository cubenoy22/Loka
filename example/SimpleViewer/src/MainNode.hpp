#ifndef LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
#define LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP

#include "app/scene/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/ImageView.hpp"
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
    loka::core::MutableState<bool> *isDialogShown_;
    loka::core::EmitterState *openDialogEvent_;
    loka::core::State<loka::core::String> *message_;
    loka::core::MutableState<loka::app::FileChooserResult> *result_;
    loka::core::State<loka::core::resource::Image> *image_;
    MainProps() : isDialogShown_(0), openDialogEvent_(0), message_(0), result_(0), image_(0) {}

    MainProps &isDialogShown(loka::core::MutableState<bool> *state)
    {
      this->isDialogShown_ = state;
      return *this;
    }

    MainProps &message(loka::core::State<loka::core::String> *state)
    {
      this->message_ = state;
      return *this;
    }

    MainProps &openDialogEvent(loka::core::EmitterState *eventState)
    {
      this->openDialogEvent_ = eventState;
      return *this;
    }

    MainProps &result(loka::core::MutableState<loka::app::FileChooserResult> *state)
    {
      this->result_ = state;
      return *this;
    }

    MainProps &image(loka::core::State<loka::core::resource::Image> *state)
    {
      this->image_ = state;
      return *this;
    }

    void assertInitialized() const
    {
      assert(this->isDialogShown_);
      assert(this->openDialogEvent_);
      assert(this->message_);
      assert(this->result_);
      assert(this->image_);
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      if (isDialogShown_ != other.isDialogShown_)
        return isDialogShown_ < other.isDialogShown_;
      if (openDialogEvent_ != other.openDialogEvent_)
        return openDialogEvent_ < other.openDialogEvent_;
      if (message_ != other.message_)
        return message_ < other.message_;
      if (result_ != other.result_)
        return result_ < other.result_;
      return image_ < other.image_;
    }
  };

  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
  public:
    typedef MainTypeTag TypeTag;
    MainNode(const MainProps &p)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(p) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      c.declare(
          VStack().alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING)
          << F()
          << Button("Open...").onClick(this->props.openDialogEvent_)
          << Text("Loka file:")
          << Text(this->props.message_).attr(TextAttr().wrap(TEXT_WRAP_CHAR).truncation(TEXT_TRUNCATION_NONE))
          << ImageView()
                 .image(this->props.image_)
                 .attr(ImageViewAttr().sizePolicy(IMAGE_VIEW_SIZE_FILL_PARENT).fit(IMAGE_FIT_CONTAIN))
          << (Show(*this->props.isDialogShown_)
              << OpenFileDialog()
                     .result(this->props.result_)));
    }
  };
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
