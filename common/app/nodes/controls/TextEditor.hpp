#ifndef LOKA_APP_TEXT_EDITOR_HPP
#define LOKA_APP_TEXT_EDITOR_HPP
#include "app/scene/Node.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/scene/state/Reported.hpp"
#include "app/style/LineHighlighter.hpp"
#include "app/nodes/controls/TextEditorDocument.hpp"
class NullTextEditorContext;
class ToolboxTextEditorContext;
class Win32TextEditorContext;
class MacTextEditorContext;
namespace loka
{
  namespace app
  {
    namespace testing
    {
      class TextEditorAccess;
    }
    struct TextEditorTypeTag
    {
    };
    class TextEditorNode;
    /** App-owned lines, fact, request and optional highlighter outlive the node.
        List, fact and request use the same owner tracker. One request slot has
        one consumer; requests resolve ItemIds against its current binding. Lines contain ASCII with
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
      scene::WriteSeat<LineCursor> moveCaretTo_;

    private:
      friend class TextEditorDocument;
      scene::WriteSeat<LineCursor> cursor_;

    public:
      const LineHighlighter *highlighter_;
      TextEditorProps()
          : lines_(0),
            moveCaretTo_(),
            cursor_(),
            highlighter_(0)
      {
      }
      TextEditorProps(core::ObservableList<core::String> &lines, scene::Reported<LineCursor> &cursor)
          : lines_(&lines),
            moveCaretTo_(),
            cursor_(this->reportSeat(cursor)),
            highlighter_(0)
      {
      }
      /** None is an empty request slot; it cannot request an absent caret. */
      TextEditorProps &moveCaretTo(const scene::NodeState<LineCursor> &value)
      {
        this->moveCaretTo_ = value.writeSeat();
        return *this;
      }
      /** Committed logical caret; this does not promise a native selection. */
      core::State<LineCursor> *cursorState() const
      {
        return this->cursor_.state();
      }
      bool cursorUsesTracker(const core::StateTracker *tracker) const
      {
        return this->cursor_.isValid() && this->cursor_.usesTracker(tracker);
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
        if (this->moveCaretTo_.state() != other.moveCaretTo_.state())
          return this->moveCaretTo_.state() < other.moveCaretTo_.state();
        return this->highlighter_ < other.highlighter_;
      }
    };
    class TextEditorNode : public scene::Node, public scene::IProjectedLayoutNode
    {
    public:
      typedef TextEditorTypeTag TypeTag;
      TextEditorProps props;

    private:
      friend class ::NullTextEditorContext;
      friend class ::ToolboxTextEditorContext;
      friend class ::Win32TextEditorContext;
      friend class ::MacTextEditorContext;
      friend class testing::TextEditorAccess;
      friend struct scene::NodePropsApplier<TextEditorNode, TextEditorProps>;
      TextEditorDocument document;

      void discardPendingRequest()
      {
        const scene::WriteSeat<LineCursor> request = this->props.moveCaretTo_;
        if (request.isValid() && !request.state()->get().isNone())
          request.set(LineCursor::None());
      }

      bool applyProps(const TextEditorProps &next)
      {
#ifndef NDEBUG
        // Scene queues subscriber-driven updates for the next run. This is
        // a diagnostic for direct kernel misuse, not an admission protocol.
        static const TextEditorNode *transitioning = 0;
        assert(transitioning != this && "TextEditor Props replacement is exclusive");
        const TextEditorNode *previous = transitioning;
        transitioning = this;
#endif
        if (this->props.lines_ != next.lines_ || this->props.moveCaretTo_.state() != next.moveCaretTo_.state())
          this->discardPendingRequest();
        // A repost from cancellation is admitted to next. Do not clear again.
        this->props = next;
#ifndef NDEBUG
        transitioning = previous;
#endif
        return true;
      }

    protected:
      virtual void onLifecycleFactChanged(scene::NodeLifecycleFact previous, scene::NodeLifecycleFact next)
      {
        if (previous == scene::NODE_FACT_ATTACHED && next != scene::NODE_FACT_ATTACHED)
          this->discardPendingRequest();
      }

    public:
      explicit TextEditorNode(const TextEditorProps &p)
          : props(p),
            document(this->props)
      {
      }
      virtual scene::NodeKind kind() const
      {
        return scene::NODE_KIND_TEXT_EDITOR;
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
        if (this->props.cursorState())
          registrar.markDirtyOnChange(this->props.cursorState(), scene::NODE_DIRTY_PROPS);
        if (this->props.moveCaretTo_.isValid())
          registrar.markDirtyOnChange(this->props.moveCaretTo_.state(), scene::NODE_DIRTY_PROPS);
      }
    };
    namespace scene
    {
      template <> struct NodePropsApplier<TextEditorNode, TextEditorProps>
      {
        static bool apply(TextEditorNode *node, const TextEditorProps &props)
        {
          return node->applyProps(props);
        }
      };
    } // namespace scene
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
      TextEditorDefinition(core::ObservableList<core::String> &lines, scene::Reported<LineCursor> &cursor)
          : scene::NodeDefinition<TextEditorProps, TextEditorNode>(TextEditorProps(lines, cursor))
      {
      }
      TextEditorDefinition &moveCaretTo(const scene::NodeState<LineCursor> &value)
      {
        this->props.moveCaretTo(value);
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
