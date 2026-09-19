#include "AttributedTextTests.hpp"
#include "app/nodes/AttributedText.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/context/NullAttributedTextContext.hpp"
#include "support/LokaAllocFailure.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  using loka::dsl::testing::SceneTestAccess;

  NullTextMeasurement measured(const AttributedString &value, short width = 200, const BlockStyle &block = BlockStyle())
  {
    NullScenePlatformController controller;
    AttributedTextNode node((AttributedText(value) + block).props);
    LayoutState state;
    state.width = width;
    controller.projectLayoutForTesting(&node, state);
    NullAttributedTextContext *context = static_cast<NullAttributedTextContext *>(node.getContext());
    LOKA_VERIFY(context != 0);
    return context->measurement();
  }

  class Registrar : public DirtySourceRegistrar
  {
  public:
    Registrar()
        : calls(0),
          source(0),
          flags(NODE_DIRTY_NONE)
    {
    }
    virtual void markDirtyOnChange(StateBase *state, NodeDirtyFlags dirty)
    {
      ++this->calls;
      this->source = state;
      this->flags = dirty;
    }
    int calls;
    StateBase *source;
    NodeDirtyFlags flags;
  };

  class Editor;
  struct EditorProps : NodePropsBase<EditorProps>
  {
    typedef Editor NodeType;
    typedef Editor TypeTag;
    explicit EditorProps(State<AttributedString> *source = 0)
        : value(source)
    {
    }
    bool operator<(const PropsBase &rhs) const
    {
      return this->value < static_cast<const EditorProps &>(rhs).value;
    }
    State<AttributedString> *value;
  };
  class Editor : public StdCompositionBoundaryNodeBase<EditorProps>
  {
  public:
    explicit Editor(const EditorProps &input)
        : StdCompositionBoundaryNodeBase<EditorProps>(input)
    {
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(AttributedText(this->props.value));
    }
  };
  class Presenter : public NullScenePlatformController
  {
  public:
    Presenter()
        : refuseProjection(false),
          commits(0),
          flags(NODE_DIRTY_NONE)
    {
    }
    virtual void onPaintCommitted()
    {
      ++this->commits;
    }
    virtual void onChange(Node *root, NodeDirtyFlags dirtyFlags, bool)
    {
      this->flags = dirtyFlags;
      LayoutState state;
      state.width = 200;
      state.height = 200;
      this->projectLayoutForTesting(root, state);
    }
    virtual bool prepareProjectedLayout(Node *node, LayoutState &state)
    {
      if (this->refuseProjection && node->asAttributedTextNode())
        return false;
      return NullScenePlatformController::prepareProjectedLayout(node, state);
    }
    bool refuseProjection;
    unsigned commits;
    NodeDirtyFlags flags;
  };
  void settle(Scene &scene)
  {
    for (int i = 0; scene.hasPendingInvalidation() && i < 12; ++i)
      LOKA_VERIFY(scene.flushInvalidation());
    LOKA_VERIFY(!scene.hasPendingInvalidation());
  }
  AttributedTextNode *textIn(Node *node)
  {
    if (node->asAttributedTextNode())
      return node->asAttributedTextNode();
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
      if (AttributedTextNode *found = textIn(child))
        return found;
    return 0;
  }
  AttributedString invalidValue()
  {
    const String input("refused");
    loka::core::testing::failLokaAllocRaw("AttributedString", "Segments", 1);
    const AttributedString invalid = Styled(input, Bold);
    loka::core::testing::allowLokaAllocRaw();
    LOKA_VERIFY(!invalid.valid());
    return invalid;
  }
} // namespace

void testAttributedTextMetrics()
{
  const AttributedString mixed = Styled("ab", FontSize<12>()) + Styled("cd", FontSize<24>());
  const NullTextMeasurement line = measured(mixed);
  LOKA_VERIFY(line.width() == 24);
  LOKA_VERIFY(line.height() == 24);
  LOKA_VERIFY(line.lineCount() == 1);
  LOKA_VERIFY(measured(Styled("ab", FontSize<24>()) + Styled("cd", FontSize<12>())).height() == 24);
  LOKA_VERIFY(measured(Styled("wo", Bold) + Styled("rd", Italic)).width() == 16);
  LOKA_VERIFY(measured(AttributedString()).height() == 12);
  LOKA_VERIFY(measured(AttributedString()).width() == 0);
  LOKA_VERIFY(measured(Styled("a\n\nb", TextStyle())).height() == 36);
  LOKA_VERIFY(measured(Styled("a\r", TextStyle()) + Styled("\nb", Bold)).lineCount() == 2);
  LOKA_VERIFY(measured(Styled("a\n", FontSize<24>()) + Styled("b", FontSize<12>())).height() == 36);
  LOKA_VERIFY(measured(Styled("\n", FontSize<24>())).height() == 48);
  LOKA_VERIFY(measured(Styled("x", FontSize<9>())).height() == 9);
  LOKA_VERIFY(measured(Styled("x", FontSize<10>())).width() == 4);
  LOKA_VERIFY(measured(Styled("a\nb", FontSize<24>())).height() == 48);
  for (int mode = TEXT_WRAP_WORD; mode <= TEXT_WRAP_CHAR; ++mode)
  {
    const BlockStyle wrap = BlockStyle().wrap(static_cast<TextWrap>(mode));
    const NullTextMeasurement wrapped = measured(mixed, 20, wrap);
    LOKA_VERIFY(wrapped.width() == 16);
    LOKA_VERIFY(wrapped.height() == 48);
    LOKA_VERIFY(wrapped.lineCount() == 2);
    // A single segment participates in the shared word/character wrapping.
    LOKA_VERIFY(measured(Styled("ab cd", Bold), 8, wrap).width() == 8);
    LOKA_VERIFY(measured(Styled("ab cd", Bold), 8, wrap).lineCount() == 3);
  }
  LOKA_VERIFY(measured(mixed, 20).lineCount() == 1);
  LOKA_VERIFY(measured(mixed, 21, BlockStyle().truncation(TEXT_TRUNCATION_CLIP)).width() == 21);
  LOKA_VERIFY(measured(mixed, 21, BlockStyle().truncation(TEXT_TRUNCATION_ELLIPSIS)).width() == 16);
  LOKA_VERIFY(measured(mixed, 21, BlockStyle().truncation(TEXT_TRUNCATION_NONE)).width() == 24);
}

void testAttributedTextPropsIdentity()
{
  const AttributedString value = Styled("ab", Bold);
  const AttributedTextProps owned(value);
  const AttributedTextProps equivalent(Styled("a", Bold) + Styled("b", Bold));
  const AttributedTextProps different(Styled("ac", Bold));
  LOKA_VERIFY(!(owned < equivalent) && !(equivalent < owned));
  LOKA_VERIFY(owned < different);
  LOKA_VERIFY(!(different < owned));
  MutableState<AttributedString> live[2];
  const AttributedTextProps borrowed(&live[0]), other(&live[1]);
  LOKA_VERIFY(borrowed < other);
  LOKA_VERIFY(!(other < borrowed));
  LOKA_VERIFY(borrowed < owned);
  LOKA_VERIFY(!(owned < borrowed));
  LOKA_VERIFY(!(borrowed < AttributedTextProps(&live[0])));
  LOKA_VERIFY(!(owned < TextProps("ab")));
  AttributedTextProps styled(owned);
  styled.blockStyle_ = BlockStyle().wrap(TEXT_WRAP_WORD);
  LOKA_VERIFY((owned < styled) == (owned.blockStyle_ < styled.blockStyle_));
  LOKA_VERIFY((styled < owned) == (styled.blockStyle_ < owned.blockStyle_));
  LOKA_VERIFY((owned < styled) != (styled < owned));
  LOKA_VERIFY(styled.text_ == &styled.ownedText);
  AttributedTextProps assigned;
  assigned = styled;
  LOKA_VERIFY(assigned.text_ == &assigned.ownedText);
  assigned = borrowed;
  LOKA_VERIFY(assigned.text_ == &live[0] && !assigned.ownsText);
  assigned.text(value);
  LOKA_VERIFY(assigned.text_ == &assigned.ownedText && assigned.ownsText);
  assigned.text(&live[1]);
  LOKA_VERIFY(assigned.text_ == &live[1] && !assigned.ownsText);
  AttributedText definition(value);
  definition.testId("EditorLine");
  definition.setNodeTag(41);
  const AttributedTextDefinitionWithAttr result =
      definition + BlockStyle().wrap(TEXT_WRAP_WORD) + BlockStyle().truncation(TEXT_TRUNCATION_CLIP);
  LOKA_VERIFY(result.hasTestId() && result.testIdValue() == "EditorLine");
  LOKA_VERIFY(result.nodeTag() == 41);
  LOKA_VERIFY(result.props.blockStyle_.wrap_ == TEXT_WRAP_WORD);
  LOKA_VERIFY(result.props.blockStyle_.truncation_ == TEXT_TRUNCATION_CLIP);
  AttributedTextNode node(result.props);
  LOKA_VERIFY(node.asTextNode() == 0);
  LOKA_VERIFY(node.asAttributedTextNode() == &node);
  LOKA_VERIFY(node.kind() == NODE_KIND_ATTRIBUTED_TEXT);
  LOKA_VERIFY(node.nodeTypeKey() == NodeTypeToken<AttributedTextNode>());
  LOKA_VERIFY(node.asProjectedLayoutNode() == &node);
}

void testAttributedTextDirtySeatsAndEditorPresentation()
{
  MutableState<AttributedString> live(Styled("var ", Bold) + Styled("x", Italic) + Styled(" = 1;", TextStyle()));
  Presenter controller;
  AttributedTextNode borrowed((AttributedTextProps(&live)));
  Registrar registrar;
  borrowed.declareDirtySources(registrar);
  LOKA_VERIFY(registrar.calls == 1 && registrar.source == &live);
  LOKA_VERIFY(registrar.flags == (NODE_DIRTY_PROPS | NODE_DIRTY_LAYOUT));
  AttributedTextNode owned((AttributedTextProps(live.get())));
  Registrar constant;
  owned.declareDirtySources(constant);
  LOKA_VERIFY(constant.calls == 0);
  Scene scene((Boundary<Editor>(EditorProps(&live))));
  scene.mount(&controller);
  SceneTestAccess::updateAttached(scene, true);
  settle(scene);
  AttributedTextNode *node = textIn(SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(node != 0);
  NullAttributedTextContext *context = static_cast<NullAttributedTextContext *>(node->getContext());
  LOKA_VERIFY(context != 0);
  LOKA_VERIFY(context->measurement().width() == 40 && context->measurement().height() == 12);
  LOKA_VERIFY(controller.commits > 0);
  const PaintQuery query = {controller.paintScope(), PLACEMENT_ELIGIBLE};
  LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
  LOKA_VERIFY(context->queryPaintDamage(query).damage.width == 0);
  const unsigned before = controller.commits;
  {
    StateTrackerGuard guard(SceneTestAccess::rootBoundary(scene)->tracker());
    live.set(live.get() + FontSize<24>());
  }
  settle(scene);
  LOKA_VERIFY(controller.commits > before);
  LOKA_VERIFY((controller.flags & NODE_DIRTY_LAYOUT) != 0);
  LOKA_VERIFY(context->measurement().height() == 24 && context->measurement().width() == 80);
  LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
  SceneTestAccess::unmount(scene);
}

void testAttributedTextInvalidProjectionRefusesAndRecovers()
{
  MutableState<AttributedString> live(Styled("before", Bold));
  Presenter controller;
  Scene scene((Boundary<Editor>(EditorProps(&live))));
  scene.mount(&controller);
  SceneTestAccess::updateAttached(scene, true);
  settle(scene);
  AttributedTextNode *node = textIn(SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(node != 0);
  NullAttributedTextContext *context = static_cast<NullAttributedTextContext *>(node->getContext());
  LOKA_VERIFY(context != 0);
  const unsigned before = controller.commits;
  {
    StateTrackerGuard guard(SceneTestAccess::rootBoundary(scene)->tracker());
    live.set(invalidValue());
  }
  settle(scene);
  LOKA_VERIFY(controller.commits == before);
  LOKA_VERIFY(context->measurement().width() == 0);
  LOKA_VERIFY(context->measurement().height() == 12);
  const PaintQuery query = {controller.paintScope(), PLACEMENT_ELIGIBLE};
  PaintAnswer answer;
  LOKA_VERIFY(controller.queryPaintAnswer(node, context, query, answer));
  LOKA_VERIFY(answer.kind == PAINT_ANSWER_REFUSED && answer.reason == PAINT_REFUSED_PROPS_UNRECONCILED);
  LOKA_VERIFY(!context->commitPresented(live.get(), controller.paintScope()));
  {
    StateTrackerGuard guard(SceneTestAccess::rootBoundary(scene)->tracker());
    live.set(Styled("after", Italic));
  }
  settle(scene);
  LOKA_VERIFY(controller.commits > before);
  LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
  context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
  LOKA_VERIFY(!context->commitPresented(live.get(), controller.paintScope()));
  context->onFactChanged(NODE_FACT_DETACHED_RETAINED, NODE_FACT_ATTACHED);
  LOKA_VERIFY(!context->commitPresented(live.get(), controller.paintScope()));
  LayoutState restored;
  restored.width = 200;
  restored.height = 200;
  controller.projectLayoutForTesting(SceneTestAccess::rootBoundary(scene), restored);
  LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
  // A skipped placement cannot retain a drawable seat from an earlier pass.
  controller.refuseProjection = true;
  LayoutState state;
  state.width = 200;
  state.height = 200;
  controller.projectLayoutForTesting(SceneTestAccess::rootBoundary(scene), state);
  LOKA_VERIFY(!context->commitPresented(live.get(), controller.paintScope()));
  SceneTestAccess::unmount(scene);
}

void testAttributedTextHandlerCannotBeReplaced()
{
  NullScenePlatformController controller;
  RefusedNodeHandler foreign(NodeTypeToken<AttributedTextNode>());
  LOKA_VERIFY(!controller.registerNodeHandler(&foreign));
  AttributedTextNode node((AttributedTextProps(Styled("x", Bold))));
  LayoutState state;
  state.width = 100;
  LOKA_VERIFY(controller.prepareProjectedLayout(&node, state));
  LOKA_VERIFY(node.getContext() != 0);
  LOKA_VERIFY(foreign.refusalCount() == 0);
  RefusedNodeHandler nativeRefusal(NodeTypeToken<AttributedTextNode>());
  LOKA_VERIFY(nativeRefusal.ensureContext(&node, &controller, state) == 0);
  LOKA_VERIFY(nativeRefusal.refusalCount() == 1);
}

void testAttributedTextSegmentationIndependentLayout()
{
  const AttributedString whole = Styled("abcd", Bold);
  const AttributedString split = Styled("ab", Bold) + Styled("cd", Bold);
  LOKA_VERIFY(whole == split);
  for (int mode = TEXT_WRAP_WORD; mode <= TEXT_WRAP_CHAR; ++mode)
  {
    const BlockStyle wrap = BlockStyle().wrap(static_cast<TextWrap>(mode));
    const NullTextMeasurement joined = measured(whole, 12, wrap);
    const NullTextMeasurement divided = measured(split, 12, wrap);
    LOKA_VERIFY(joined.width() == divided.width());
    LOKA_VERIFY(joined.height() == divided.height());
    LOKA_VERIFY(joined.lineCount() == divided.lineCount());
    LOKA_VERIFY(joined.width() == 12 && joined.height() == 24 && joined.lineCount() == 2);
  }
  for (int mode = TEXT_TRUNCATION_CLIP; mode <= TEXT_TRUNCATION_ELLIPSIS; ++mode)
  {
    const BlockStyle truncate = BlockStyle().truncation(static_cast<TextTruncation>(mode));
    const NullTextMeasurement joined = measured(whole, 11, truncate);
    const NullTextMeasurement divided = measured(split, 11, truncate);
    LOKA_VERIFY(joined.width() == divided.width());
    LOKA_VERIFY(joined.height() == divided.height());
    LOKA_VERIFY(joined.lineCount() == divided.lineCount());
    LOKA_VERIFY(joined.width() == (mode == TEXT_TRUNCATION_CLIP ? 11 : 8));
  }
}

void testAttributedTextWrapUsesJoinedWordsAndRunMetrics()
{
  // "abcd" fits a complete line. WORD moves it intact, even though its
  // character metrics change in the middle; CHAR fills the previous line.
  const AttributedString words = Styled("a ab", FontSize<12>()) + Styled("cd", FontSize<24>());
  const NullTextMeasurement word = measured(words, 24, BlockStyle().wrap(TEXT_WRAP_WORD));
  LOKA_VERIFY(word.width() == 24 && word.height() == 36 && word.lineCount() == 2);
  const NullTextMeasurement character = measured(words, 24, BlockStyle().wrap(TEXT_WRAP_CHAR));
  LOKA_VERIFY(character.width() == 24 && character.height() == 48 && character.lineCount() == 2);
  const AttributedString sameStyle = Styled("a ab", Bold) + Styled("cd", Bold);
  const NullTextMeasurement intact = measured(sameStyle, 16, BlockStyle().wrap(TEXT_WRAP_WORD));
  LOKA_VERIFY(intact.width() == 16 && intact.height() == 24 && intact.lineCount() == 2);
  // A size change halfway through a forced word break contributes to both lines.
  const AttributedString longWord = Styled("ab", FontSize<12>()) + Styled("cd", FontSize<24>());
  const NullTextMeasurement forced = measured(longWord, 20, BlockStyle().wrap(TEXT_WRAP_WORD));
  LOKA_VERIFY(forced.width() == 16 && forced.height() == 48 && forced.lineCount() == 2);
}
