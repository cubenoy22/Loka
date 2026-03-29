#ifndef LOKA_APP_OPEN_FILE_DIALOG_HPP
#define LOKA_APP_OPEN_FILE_DIALOG_HPP

#include "loka/core/State.hpp"
#include "app/scene/Node.hpp"
#include "file/File.hpp"

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

      FileChooserResult() : kind(RESULT_NONE), item(), errorCode(0) {}

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

    class OpenFileDialogNode;

    struct OpenFileDialogProps : public loka::app::scene::NodePropsBase<OpenFileDialogProps>
    {
      typedef OpenFileDialogTypeTag TypeTag;
      typedef OpenFileDialogNode NodeType;
      loka::core::MutableState<bool> *isVisible_;
      loka::core::MutableState<FileChooserResult> *result_;
      loka::core::EmitterState *onResult_;
      void *windowToAttach_;
      OpenFileDialogProps() : isVisible_(0), result_(0), onResult_(0), windowToAttach_(0) {}

      OpenFileDialogProps &isVisible(loka::core::MutableState<bool> *state)
      {
        this->isVisible_ = state;
        return *this;
      }

      OpenFileDialogProps &result(loka::core::MutableState<FileChooserResult> *state)
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
        if (isVisible_ != other.isVisible_)
          return isVisible_ < other.isVisible_;
        if (result_ != other.result_)
          return result_ < other.result_;
        if (onResult_ != other.onResult_)
          return onResult_ < other.onResult_;
        return windowToAttach_ < other.windowToAttach_;
      }
    };

    class OpenFileDialogNode : public loka::app::scene::Node
    {
    public:
      typedef OpenFileDialogTypeTag TypeTag;
      OpenFileDialogProps props;
      OpenFileDialogNode(const OpenFileDialogProps &p) : props(p) {}
      virtual loka::app::scene::NodeKind kind() const { return loka::app::scene::NODE_KIND_OPEN_FILE_DIALOG; }
      virtual const void *nodeTypeKey() const { return loka::app::scene::NodeTypeToken<OpenFileDialogNode>(); }
      virtual OpenFileDialogNode *asOpenFileDialogNode() { return this; }
      virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
      {
        (void)registrar;
      }
    };

    struct OpenFileDialogDefinition : public loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>, public loka::app::scene::TestIdDslMixin<OpenFileDialogDefinition>
    {
      OpenFileDialogDefinition() : loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>() {}
      OpenFileDialogDefinition(const OpenFileDialogProps &p) : loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>(p) {}

      OpenFileDialogDefinition &isVisible(loka::core::MutableState<bool> *state)
      {
        this->props.isVisible_ = state;
        return *this;
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

      OpenFileDialogDefinition &onResult(loka::core::EmitterState *emitter)
      {
        this->props.onResult_ = emitter;
        return *this;
      }
      using loka::app::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>::create;
    };

    typedef OpenFileDialogDefinition OpenFileDialog;
  } // namespace app
} // namespace loka

#endif // LOKA_APP_OPEN_FILE_DIALOG_HPP
