#ifndef LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
#define LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "app/Empty.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include <cassert>

namespace simpleviewer
{
  class MainTypeTag
  {
  };

  class MainNode;

  struct MainProps : public declara::core::scene::NodePropsBase<MainProps>
  {
    typedef MainTypeTag TypeTag;
    typedef MainNode NodeType;
    State<bool> *dialogVisible_;
    State<loka::core::String> *message_;
    MutableState<declara::app::FileChooserResult> *result_;
    EmitterState *onResult_;
    MainProps() : dialogVisible_(0), message_(0), result_(0), onResult_(0) {}

    MainProps &dialogVisible(State<bool> *state)
    {
      this->dialogVisible_ = state;
      return *this;
    }

    MainProps &message(State<loka::core::String> *state)
    {
      this->message_ = state;
      return *this;
    }

    MainProps &result(MutableState<declara::app::FileChooserResult> *state)
    {
      this->result_ = state;
      return *this;
    }

    MainProps &onResult(EmitterState *emitter)
    {
      this->onResult_ = emitter;
      return *this;
    }

    void assertInitialized() const
    {
      assert(this->dialogVisible_);
      assert(this->message_);
      assert(this->result_);
      assert(this->onResult_);
    }

    bool operator<(const declara::core::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      if (dialogVisible_ != other.dialogVisible_)
        return dialogVisible_ < other.dialogVisible_;
      if (message_ != other.message_)
        return message_ < other.message_;
      if (result_ != other.result_)
        return result_ < other.result_;
      return onResult_ < other.onResult_;
    }
  };

  class MainNode : public declara::core::scene::StaticCompositionBoundaryNodeBase<MainProps>
  {
  public:
    typedef MainTypeTag TypeTag;
    MainNode(const MainProps &p)
        : declara::core::scene::StaticCompositionBoundaryNodeBase<MainProps>(p) {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      this->props.assertInitialized();
      OpenFileDialog dialog;
      dialog.isVisible(this->props.dialogVisible_);
      dialog.result(this->props.result_);
      dialog.onResult(this->props.onResult_);
      c.declare(
          VStack()
          << Empty()
          << Text("You chose:")
          << Text(this->props.message_)
          << dialog);
    }
  };
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
