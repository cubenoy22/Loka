#ifndef LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
#define LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP

#include "app/scene/node/StaticComposition.hpp"
#include "app/Empty.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/ImageView.hpp"
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
    loka::core::State<loka::core::String> *message_;
    loka::core::MutableState<loka::app::FileChooserResult> *result_;
    loka::core::State<loka::core::resource::Image> *image_;
    MainProps() : isDialogShown_(0), message_(0), result_(0), image_(0) {}

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
      if (message_ != other.message_)
        return message_ < other.message_;
      if (result_ != other.result_)
        return result_ < other.result_;
      return image_ < other.image_;
    }
  };

  class MainNode : public loka::app::scene::StaticCompositionBoundaryNodeBase<MainProps>
  {
  public:
    typedef MainTypeTag TypeTag;
    MainNode(const MainProps &p)
        : loka::app::scene::StaticCompositionBoundaryNodeBase<MainProps>(p) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      c.declare(
          VStack()
          << Empty()
          << Text("Loka file:")
          << Text(this->props.message_)
          << ImageView()
                 .image(this->props.image_)
                 .size(0, 180)
          << OpenFileDialog()
                 .isVisible(this->props.isDialogShown_)
                 .result(this->props.result_));
    }
  };
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
