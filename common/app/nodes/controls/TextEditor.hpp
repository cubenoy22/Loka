#ifndef LOKA_APP_TEXT_EDITOR_HPP
#define LOKA_APP_TEXT_EDITOR_HPP
#include "app/scene/Node.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/scene/state/WriteSeat.hpp"
#include "app/style/LineHighlighter.hpp"
#include "app/nodes/controls/TextEditorDocument.hpp"
namespace loka
{
  namespace app
  {
    struct TextEditorTypeTag
    {
    };
    class TextEditorNode;
    /** App-owned lines, cursor and optional highlighter must outlive the node.
        List and cursor must have the same owner tracker. Lines contain ASCII with
        no embedded CR/LF/NUL; separators count toward the whole-document byte cap. */
    struct TextEditorProps : scene::NodePropsBase<TextEditorProps>
    {
      typedef TextEditorTypeTag TypeTag;
      typedef TextEditorNode NodeType;
      enum
      {
        kMaxLines = 256,
        kMaxBytes = 8192
      };
      core::ObservableList<core::String> *lines_;
      scene::WriteSeat<LineCursor> cursor_;
      const LineHighlighter *highlighter_;
      TextEditorProps()
          : lines_(0),
            cursor_(),
            highlighter_(0)
      {
      }
      TextEditorProps(core::ObservableList<core::String> &lines, const scene::NodeState<LineCursor> &cursor)
          : lines_(&lines),
            cursor_(cursor.writeSeat()),
            highlighter_(0)
      {
      }
      TextEditorProps &cursor(const scene::NodeState<LineCursor> &value)
      {
        this->cursor_ = value.writeSeat();
        return *this;
      }
      TextEditorProps &highlighter(const LineHighlighter &value)
      {
        this->highlighter_ = &value;
        return *this;
      }
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != this->propsTypeId())
          return false;
        const TextEditorProps &other = static_cast<const TextEditorProps &>(rhs);
        if (this->lines_ != other.lines_)
          return this->lines_ < other.lines_;
        if (this->cursor_.state() != other.cursor_.state())
          return this->cursor_.state() < other.cursor_.state();
        return this->highlighter_ < other.highlighter_;
      }
    };
    class TextEditorNode : public scene::Node, public scene::IProjectedLayoutNode
    {
    public:
      typedef TextEditorTypeTag TypeTag;
      TextEditorProps props;
      TextEditorDocument document;
      explicit TextEditorNode(const TextEditorProps &p)
          : props(p),
            document(this->props)
      {
      }
      virtual const void *nodeTypeKey() const
      {
        return scene::NodeTypeToken<TextEditorNode>();
      }
      virtual scene::IProjectedLayoutNode *asProjectedLayoutNode()
      {
        return this;
      }
      virtual short layoutProjected(scene::IPlatformController *controller, scene::LayoutState &state)
      {
        assert(this->props.lines_ && "TextEditor borrows a non-null app-owned list");
        if (!controller || !scene::PrepareProjectedLayout(controller, this, state))
          return state.y;
        return scene::Node::layout(controller, state);
      }
      virtual void declareDirtySources(scene::DirtySourceRegistrar &registrar)
      {
        assert(this->props.lines_);
        if (this->props.lines_)
          registrar.markDirtyOnChange(const_cast<core::State<core::ListRevision> *>(&this->props.lines_->revision()),
                                      scene::NODE_DIRTY_PROPS);
        if (this->props.cursor_.isValid())
          registrar.markDirtyOnChange(this->props.cursor_.state(), scene::NODE_DIRTY_PROPS);
      }
    };
    struct TextEditorDefinition : scene::NodeDefinition<TextEditorProps, TextEditorNode>,
                                  scene::TestIdDslMixin<TextEditorDefinition>
    {
      TextEditorDefinition()
          : scene::NodeDefinition<TextEditorProps, TextEditorNode>()
      {
      }
      explicit TextEditorDefinition(const TextEditorProps &p)
          : scene::NodeDefinition<TextEditorProps, TextEditorNode>(p)
      {
      }
      TextEditorDefinition(core::ObservableList<core::String> &lines, const scene::NodeState<LineCursor> &cursor)
          : scene::NodeDefinition<TextEditorProps, TextEditorNode>(TextEditorProps(lines, cursor))
      {
      }
      TextEditorDefinition &cursor(const scene::NodeState<LineCursor> &value)
      {
        this->props.cursor(value);
        return *this;
      }
      TextEditorDefinition &highlighter(const LineHighlighter &value)
      {
        this->props.highlighter(value);
        return *this;
      }
    };
    typedef TextEditorDefinition TextEditor;
  } // namespace app
} // namespace loka
#endif
