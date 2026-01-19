#ifndef LOKA_APP_OPEN_FILE_DIALOG_HPP
#define LOKA_APP_OPEN_FILE_DIALOG_HPP

#include "core/State.hpp"
#include "core2/scene/Node.hpp"

namespace declara
{
  namespace app
  {
    class OpenFileDialogTypeTag
    {
    };

    class OpenFileDialogNode;

    struct OpenFileDialogProps : public declara::core::scene::NodePropsBase<OpenFileDialogProps>
    {
      typedef OpenFileDialogTypeTag TypeTag;
      typedef OpenFileDialogNode NodeType;
      State<bool> *isVisible_;
      void *windowToAttach_;
      OpenFileDialogProps() : isVisible_(0), windowToAttach_(0) {}

      OpenFileDialogProps &isVisible(State<bool> *state)
      {
        this->isVisible_ = state;
        return *this;
      }

      OpenFileDialogProps &attachToWindow(void *window)
      {
        this->windowToAttach_ = window;
        return *this;
      }

      bool operator<(const declara::core::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const OpenFileDialogProps &other = static_cast<const OpenFileDialogProps &>(rhs);
        if (isVisible_ != other.isVisible_)
          return isVisible_ < other.isVisible_;
        return windowToAttach_ < other.windowToAttach_;
      }
    };

    class OpenFileDialogNode : public declara::core::scene::Node
    {
    public:
      typedef OpenFileDialogTypeTag TypeTag;
      OpenFileDialogProps props;
      OpenFileDialogNode(const OpenFileDialogProps &p) : props(p) {}
      virtual declara::core::scene::NodeKind kind() const { return declara::core::scene::NODE_KIND_OPEN_FILE_DIALOG; }
      virtual OpenFileDialogNode *asOpenFileDialogNode() { return this; }
    };

    struct OpenFileDialogDefinition : public declara::core::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>
    {
      OpenFileDialogDefinition() : NodeDefinition() {}
      OpenFileDialogDefinition(const OpenFileDialogProps &p) : NodeDefinition(p) {}

      OpenFileDialogDefinition &isVisible(State<bool> *state)
      {
        this->props.isVisible_ = state;
        return *this;
      }

      OpenFileDialogDefinition &attachToWindow(void *window)
      {
        this->props.windowToAttach_ = window;
        return *this;
      }

      using declara::core::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>::create;
    };

    typedef OpenFileDialogDefinition OpenFileDialog;
  } // namespace app
} // namespace declara

#endif // LOKA_APP_OPEN_FILE_DIALOG_HPP
