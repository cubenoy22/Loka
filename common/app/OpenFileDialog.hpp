#ifndef LOKA_APP_OPEN_FILE_DIALOG_HPP
#define LOKA_APP_OPEN_FILE_DIALOG_HPP

#include <cassert>

#include "core/State.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/io/File.hpp"

namespace loka
{
  namespace app
  {

    enum FileDialogPurpose { FILE_DIALOG_OPEN, FILE_DIALOG_SAVE };

    /** DEFAULT preserves each rail's OPEN filters. ALL_FILES_TEXT admits all
        files, with a text filter choice where the native dialog supports it. */
    enum FileDialogFilterPolicy { FILE_DIALOG_FILTER_DEFAULT, FILE_DIALOG_FILTER_ALL_FILES_TEXT };

    /** Completed, owned options for the shared Open/Save family. Copyable and
        assignable in C++98; OPEN has no default name. */
    class FileDialogOptions
    {
    public:
      FileDialogOptions(FileDialogPurpose purpose = FILE_DIALOG_OPEN,
                        const core::String &defaultName = core::String(),
                        FileDialogFilterPolicy filter = FILE_DIALOG_FILTER_DEFAULT)
          : purpose_(purpose),
            defaultName_(purpose == FILE_DIALOG_SAVE ? defaultName : core::String()),
            filter_(filter) {}

      FileDialogPurpose purpose() const { return this->purpose_; }
      const core::String &defaultName() const { return this->defaultName_; }
      FileDialogFilterPolicy filterPolicy() const { return this->filter_; }

      int compare(const FileDialogOptions &other) const
      {
        if (this->purpose_ != other.purpose_)
          return this->purpose_ < other.purpose_ ? -1 : 1;
        if (this->filter_ != other.filter_)
          return this->filter_ < other.filter_ ? -1 : 1;
        return this->defaultName_.compare(other.defaultName_);
      }

    private:
      FileDialogPurpose purpose_;
      core::String defaultName_;
      FileDialogFilterPolicy filter_;
    };

    /** Rail has not implemented this purpose; no native dialog was shown. */
    enum FileDialogError { FILE_DIALOG_ERROR_UNSUPPORTED_PURPOSE = -1 };

    /** RESULT_FILE is a selected address, not a certificate of existence or
        writability. A SAVE destination may not exist yet. */
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

    /** Internal OpenFileDialog names serve both OPEN and SAVE. */
    struct OpenFileDialogProps : public loka::app::scene::NodePropsBase<OpenFileDialogProps>
    {
      typedef OpenFileDialogTypeTag TypeTag;
      typedef OpenFileDialogNode NodeType;
      FileDialogOptions options_;
      loka::app::scene::NodeState<FileChooserResult> result_;
      loka::core::EmitterState *onResult_;
      void *windowToAttach_;
      OpenFileDialogProps()
          : result_(),
            onResult_(0),
            windowToAttach_(0)
      {
      }

      OpenFileDialogProps &result(const loka::app::scene::NodeState<FileChooserResult> &state)
      {
        this->result_ = state;
        return *this;
      }

      OpenFileDialogProps &onResult(loka::core::EmitterState *emitter)
      {
        this->onResult_ = emitter;
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
        const int optionsOrder = this->options_.compare(other.options_);
        if (optionsOrder != 0)
          return optionsOrder < 0;
        if (result_.state() != other.result_.state())
          return result_.state() < other.result_.state();
        if (onResult_ != other.onResult_)
          return onResult_ < other.onResult_;
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
        assert((props.result_.isValid() || props.onResult_) &&
               "OpenFileDialog delivers completion only through result/onResult; "
               "bind one and close the owning Show from it");
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

      OpenFileDialogDefinition &filterPolicy(FileDialogFilterPolicy filter)
      {
        this->props.options_ = FileDialogOptions(this->props.options_.purpose(),
                                                this->props.options_.defaultName(), filter);
        return *this;
      }

      OpenFileDialogDefinition &attachToWindow(void *window)
      {
        this->props.windowToAttach_ = window;
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

      using loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>::create;
    };

    typedef OpenFileDialogDefinition OpenFileDialog;

    /** Select a SAVE destination using the existing file-dialog result door. */
    inline OpenFileDialogDefinition SaveFileDialog(const core::String &defaultName)
    {
      OpenFileDialogProps props;
      props.options_ = FileDialogOptions(FILE_DIALOG_SAVE, defaultName);
      return OpenFileDialogDefinition(props);
    }
  } // namespace app
} // namespace loka

#endif // LOKA_APP_OPEN_FILE_DIALOG_HPP
