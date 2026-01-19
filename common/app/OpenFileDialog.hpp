#ifndef LOKA_APP_OPEN_FILE_DIALOG_HPP
#define LOKA_APP_OPEN_FILE_DIALOG_HPP

#include "core/State.hpp"
#include "core2/scene/Node.hpp"
#include "loka/core/String.hpp"
#include <cstring>
#if defined(LOKA_RETRO68)
#include <Files.h>
#endif

namespace declara
{
  namespace app
  {
    struct FileRef
    {
      enum Kind
      {
        KIND_NONE = 0,
        KIND_PATH,
        KIND_FSSPEC
      };

      FileRef() : kind(KIND_NONE), path()
#if defined(LOKA_RETRO68)
                ,
                spec()
#endif
      {
      }

      static FileRef FromPath(const loka::core::String &value)
      {
        FileRef ref;
        ref.kind = KIND_PATH;
        ref.path = value;
        return ref;
      }

#if defined(LOKA_RETRO68)
      static FileRef FromFSSpec(const FSSpec &value)
      {
        FileRef ref;
        ref.kind = KIND_FSSPEC;
        ref.spec = value;
        return ref;
      }
#endif

      Kind kind;
      loka::core::String path;
#if defined(LOKA_RETRO68)
      FSSpec spec;
#endif
    };

    inline bool operator!=(const FileRef &lhs, const FileRef &rhs)
    {
      if (lhs.kind != rhs.kind)
      {
        return true;
      }
      if (lhs.kind == FileRef::KIND_PATH)
      {
        return !lhs.path.equals(rhs.path);
      }
#if defined(LOKA_RETRO68)
      if (lhs.kind == FileRef::KIND_FSSPEC)
      {
        if (lhs.spec.vRefNum != rhs.spec.vRefNum || lhs.spec.parID != rhs.spec.parID)
        {
          return true;
        }
        return std::memcmp(lhs.spec.name, rhs.spec.name, sizeof(lhs.spec.name)) != 0;
      }
#endif
      return false;
    }

    struct FolderRef
    {
      enum Kind
      {
        KIND_NONE = 0,
        KIND_PATH,
        KIND_FSSPEC
      };

      FolderRef() : kind(KIND_NONE), path()
#if defined(LOKA_RETRO68)
                  ,
                  spec()
#endif
      {
      }

      static FolderRef FromPath(const loka::core::String &value)
      {
        FolderRef ref;
        ref.kind = KIND_PATH;
        ref.path = value;
        return ref;
      }

#if defined(LOKA_RETRO68)
      static FolderRef FromFSSpec(const FSSpec &value)
      {
        FolderRef ref;
        ref.kind = KIND_FSSPEC;
        ref.spec = value;
        return ref;
      }
#endif

      Kind kind;
      loka::core::String path;
#if defined(LOKA_RETRO68)
      FSSpec spec;
#endif
    };

    inline bool operator!=(const FolderRef &lhs, const FolderRef &rhs)
    {
      if (lhs.kind != rhs.kind)
      {
        return true;
      }
      if (lhs.kind == FolderRef::KIND_PATH)
      {
        return !lhs.path.equals(rhs.path);
      }
#if defined(LOKA_RETRO68)
      if (lhs.kind == FolderRef::KIND_FSSPEC)
      {
        if (lhs.spec.vRefNum != rhs.spec.vRefNum || lhs.spec.parID != rhs.spec.parID)
        {
          return true;
        }
        return std::memcmp(lhs.spec.name, rhs.spec.name, sizeof(lhs.spec.name)) != 0;
      }
#endif
      return false;
    }

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

      FileChooserResult() : kind(RESULT_NONE), file(), folder(), errorCode(0) {}

      static FileChooserResult File(const FileRef &ref)
      {
        FileChooserResult result;
        result.kind = RESULT_FILE;
        result.file = ref;
        return result;
      }

      static FileChooserResult Folder(const FolderRef &ref)
      {
        FileChooserResult result;
        result.kind = RESULT_FOLDER;
        result.folder = ref;
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
      FileRef file;
      FolderRef folder;
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
        return lhs.file != rhs.file;
      }
      if (lhs.kind == FileChooserResult::RESULT_FOLDER)
      {
        return lhs.folder != rhs.folder;
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

    struct OpenFileDialogProps : public declara::core::scene::NodePropsBase<OpenFileDialogProps>
    {
      typedef OpenFileDialogTypeTag TypeTag;
      typedef OpenFileDialogNode NodeType;
      State<bool> *isVisible_;
      MutableState<FileChooserResult> *result_;
      EmitterState *onResult_;
      void *windowToAttach_;
      OpenFileDialogProps() : isVisible_(0), result_(0), onResult_(0), windowToAttach_(0) {}

      OpenFileDialogProps &isVisible(State<bool> *state)
      {
        this->isVisible_ = state;
        return *this;
      }

      OpenFileDialogProps &result(MutableState<FileChooserResult> *state)
      {
        this->result_ = state;
        return *this;
      }

      OpenFileDialogProps &onResult(EmitterState *emitter)
      {
        this->onResult_ = emitter;
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
        if (result_ != other.result_)
          return result_ < other.result_;
        if (onResult_ != other.onResult_)
          return onResult_ < other.onResult_;
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

      OpenFileDialogDefinition &result(MutableState<FileChooserResult> *state)
      {
        this->props.result_ = state;
        return *this;
      }

      OpenFileDialogDefinition &onResult(EmitterState *emitter)
      {
        this->props.onResult_ = emitter;
        return *this;
      }

      using declara::core::scene::NodeDefinition<OpenFileDialogProps, OpenFileDialogNode>::create;
    };

    typedef OpenFileDialogDefinition OpenFileDialog;
  } // namespace app
} // namespace declara

#endif // LOKA_APP_OPEN_FILE_DIALOG_HPP
