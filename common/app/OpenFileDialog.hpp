#ifndef LOKA_APP_OPEN_FILE_DIALOG_HPP
#define LOKA_APP_OPEN_FILE_DIALOG_HPP

#include "core/State.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/io/File.hpp"

namespace loka
{
  namespace app
  {

    struct FileChooserResult
    {
      enum Kind
      {
        RESULT_NONE = 0,
        RESULT_FILE,
        RESULT_FOLDER,
        RESULT_CANCELED,
        RESULT_ERROR
      };

      FileChooserResult()
          : kind(RESULT_NONE),
            item(),
            errorCode(0)
      {
      }

      static FileChooserResult File(const loka::file::File &value)
      {
        FileChooserResult result;
        result.kind = RESULT_FILE;
        result.item = value;
        return result;
      }

      static FileChooserResult Folder(const loka::file::File &value)
      {
        FileChooserResult result;
        result.kind = RESULT_FOLDER;
        result.item = value;
        return result;
      }

      static FileChooserResult Canceled()
      {
        FileChooserResult result;
        result.kind = RESULT_CANCELED;
        return result;
      }

      static FileChooserResult Error(int code)
      {
        FileChooserResult result;
        result.kind = RESULT_ERROR;
        result.errorCode = code;
        return result;
      }

      Kind kind;
      loka::file::File item;
      int errorCode;
    };

    inline bool operator!=(const FileChooserResult &lhs, const FileChooserResult &rhs)
    {
      if (lhs.kind != rhs.kind)
      {
        return true;
      }
      if (lhs.kind == FileChooserResult::RESULT_FILE)
      {
        return lhs.item != rhs.item;
      }
      if (lhs.kind == FileChooserResult::RESULT_FOLDER)
      {
        return lhs.item != rhs.item;
      }
      if (lhs.kind == FileChooserResult::RESULT_ERROR)
      {
        return lhs.errorCode != rhs.errorCode;
      }
      return false;
    }

    class OpenFileDialogTypeTag
    {
    };

    enum OpenFileDialogPresentationState
    {
      OPEN_FILE_DIALOG_PRESENTATION_IDLE = 0,
      OPEN_FILE_DIALOG_PRESENTATION_PENDING_ATTACH = 1,
      OPEN_FILE_DIALOG_PRESENTATION_PRESENTING = 2,
      OPEN_FILE_DIALOG_PRESENTATION_PRESENTED = 3
    };

    struct OpenFileDialogPresentationPhase
    {
      OpenFileDialogPresentationPhase()
          : value(OPEN_FILE_DIALOG_PRESENTATION_PENDING_ATTACH)
      {
      }

      bool beginPresent()
      {
        if (value == OPEN_FILE_DIALOG_PRESENTATION_PRESENTING || value == OPEN_FILE_DIALOG_PRESENTATION_PRESENTED)
        {
          return false;
        }
        value = OPEN_FILE_DIALOG_PRESENTATION_PRESENTING;
        return true;
      }

      void markPresented()
      {
        value = OPEN_FILE_DIALOG_PRESENTATION_PRESENTED;
      }

      void markDetached()
      {
        value = OPEN_FILE_DIALOG_PRESENTATION_PENDING_ATTACH;
      }

      bool isPresenting() const
      {
        return value == OPEN_FILE_DIALOG_PRESENTATION_PRESENTING;
      }

      OpenFileDialogPresentationState value;
    };

    class OpenFileDialogNode;

    struct OpenFileDialogProps : public loka::app::scene::NodePropsBase<OpenFileDialogProps>
    {
      typedef OpenFileDialogTypeTag TypeTag;
      typedef OpenFileDialogNode NodeType;
      loka::core::MutableState<FileChooserResult> *result_;
      loka::core::EmitterState *onResult_;
      loka::core::MutableState<bool> *closeState_;
      void *windowToAttach_;
      OpenFileDialogProps()
          : result_(0),
            onResult_(0),
            closeState_(0),
            windowToAttach_(0)
      {
      }

      OpenFileDialogProps &result(loka::core::MutableState<FileChooserResult> *state)
      {
        this->result_ = state;
        return *this;
      }

      OpenFileDialogProps &result(const loka::app::scene::NodeState<FileChooserResult> &state)
      {
        this->result_ = state.dangerouslyMutableState();
        return *this;
      }

      OpenFileDialogProps &onResult(loka::core::EmitterState *emitter)
      {
        this->onResult_ = emitter;
        return *this;
      }

      OpenFileDialogProps &closeState(loka::core::MutableState<bool> *state)
      {
        this->closeState_ = state;
        return *this;
      }

      OpenFileDialogProps &closeState(const loka::app::scene::NodeState<bool> &state)
      {
        this->closeState_ = state.dangerouslyMutableState();
        return *this;
      }

      OpenFileDialogProps &attachToWindow(void *window)
      {
        this->windowToAttach_ = window;
        return *this;
      }

      bool operator<(const loka::app::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const OpenFileDialogProps &other = static_cast<const OpenFileDialogProps &>(rhs);
        if (result_ != other.result_)
          return result_ < other.result_;
        if (onResult_ != other.onResult_)
          return onResult_ < other.onResult_;
        if (closeState_ != other.closeState_)
          return closeState_ < other.closeState_;
        return windowToAttach_ < other.windowToAttach_;
      }
    };

    class OpenFileDialogNode : public loka::app::scene::Node, public loka::app::scene::IProjectedLayoutNode
    {
    public:
      typedef OpenFileDialogTypeTag TypeTag;
      OpenFileDialogProps props;
      OpenFileDialogNode(const OpenFileDialogProps &p)
          : props(p)
      {
      }
      virtual loka::app::scene::NodeKind kind() const
      {
        return loka::app::scene::NODE_KIND_OPEN_FILE_DIALOG;
      }
      virtual loka::app::scene::IProjectedLayoutNode *asProjectedLayoutNode()
      {
        return this;
      }
      virtual const void *nodeTypeKey() const
      {
        return loka::app::scene::NodeTypeToken<OpenFileDialogNode>();
      }
      virtual OpenFileDialogNode *asOpenFileDialogNode()
      {
        return this;
      }
      virtual short layoutProjected(loka::app::scene::IPlatformController *controller,
                                    loka::app::scene::LayoutState &state)
      {
        if (!controller)
        {
          return state.y;
        }
        if (!loka::app::scene::PrepareProjectedLayout(controller, this, state))
        {
          return state.y;
        }
        return state.y;
      }
      virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
      {
        (void)registrar;
      }
    };

    struct OpenFileDialogDefinition : public loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>,
                                      public loka::app::scene::TestIdDslMixin<OpenFileDialogDefinition>
    {
      OpenFileDialogDefinition()
          : loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>()
      {
      }
      OpenFileDialogDefinition(const OpenFileDialogProps &p)
          : loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>(p)
      {
      }

      OpenFileDialogDefinition &attachToWindow(void *window)
      {
        this->props.windowToAttach_ = window;
        return *this;
      }

      OpenFileDialogDefinition &result(loka::core::MutableState<FileChooserResult> *state)
      {
        this->props.result_ = state;
        return *this;
      }

      OpenFileDialogDefinition &result(const loka::app::scene::NodeState<FileChooserResult> &state)
      {
        this->props.result(state);
        return *this;
      }

      OpenFileDialogDefinition &onResult(loka::core::EmitterState *emitter)
      {
        this->props.onResult_ = emitter;
        return *this;
      }

      OpenFileDialogDefinition &closeState(loka::core::MutableState<bool> *state)
      {
        this->props.closeState_ = state;
        return *this;
      }

      OpenFileDialogDefinition &closeState(const loka::app::scene::NodeState<bool> &state)
      {
        this->props.closeState(state);
        return *this;
      }
      using loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>::create;
    };

    typedef OpenFileDialogDefinition OpenFileDialog;
  } // namespace app
} // namespace loka

#endif // LOKA_APP_OPEN_FILE_DIALOG_HPP
