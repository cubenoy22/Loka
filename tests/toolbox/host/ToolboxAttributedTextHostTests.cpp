#include "ToolboxHost.hpp"
#include "ToolboxBuiltInSupport.hpp"
#include "ToolboxDirtyReplay.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "context/ToolboxAttributedTextContext.hpp"
#include "support/TestVerify.hpp"
#include "support/LokaAllocFailure.hpp"
#include <cstdio>
#include "platform/String.hpp"

namespace loka
{
  namespace testing
  {
    class ToolboxAttributedTextContextAccess
    {
    public:
      static const ToolboxAttributedTextTable &table(const ToolboxAttributedTextContext &c)
      {
        return c.table_;
      }
      static Rect rect(const ToolboxAttributedTextContext &c)
      {
        return c.rect_;
      }
      static Rect paintRect(const ToolboxAttributedTextContext &c)
      {
        return c.paintRect_;
      }
      static bool known(const ToolboxAttributedTextContext &c)
      {
        return c.presented_.isKnown();
      }
    };
  } // namespace testing
} // namespace loka

using namespace loka::app;
using namespace loka::app::scene;
using loka::testing::ToolboxAttributedTextContextAccess;
namespace
{
  LayoutState Seat(short width)
  {
    LayoutState s;
    s.x = 10;
    s.y = 20;
    s.width = width;
    s.spacing = 0;
    return s;
  }
  void Repaint(ToolboxScenePlatformController &controller, ToolboxAttributedTextContext &context)
  {
    ToolboxRenderDirtyInCompositionOrder(controller, ToolboxAttributedTextContextAccess::paintRect(context));
  }
  void Pin(const char *name)
  {
    std::printf("[pin] %s\n", name);
    std::fflush(stdout);
  }
} // namespace

int main(int argc, char **)
{
  const loka::core::Managed<loka::platform::String> utf8 = loka::platform::CreatePlatformStringFromUtf8("a\0\xff", 3);
  loka::platform::Utf8View view = {0, 0};
  LOKA_VERIFY(utf8->queryUtf8(view) && view.length == 3 && std::string(view.bytes, view.length) == std::string("a\0\xff", 3));

  ToolboxWindow window;
  ToolboxScenePlatformController controller(&window);
  LOKA_VERIFY(RegisterToolboxBuiltInSupport(controller));
  if (argc == 1)
  {
    const AttributedString value = Styled("a ab", FontSize<12>() + Bold) + Styled("cd", FontSize<24>() + Italic);
    const AttributedString updated = Styled("var x = ", Bold) + Styled("1;", Italic);
    AttributedTextNode &node =
        *new AttributedTextNode((AttributedText(value) + BlockStyle().wrap(TEXT_WRAP_WORD)).props);
    LayoutState seat = Seat(30);
    Pin("ToolboxAttributedTextInstalledAcrossSegmentWord");
    IPlatformNodeHandler *handler = controller.nodeHandlerRegistry_.find(&node);
    LOKA_VERIFY(handler);
    loka::core::testing::failLokaAllocRaw("ToolboxAttributedText", "Context", 1);
    NodeContext *refused = handler->ensureContext(&node, &controller, seat);
    LOKA_VERIFY(!refused && !node.getContext());
    NodeContext *installed = handler->ensureContext(&node, &controller, seat);
    LOKA_VERIFY(installed != 0);
    LOKA_VERIFY(controller.compositionReplay.required());
    ToolboxAttributedTextContext *context = static_cast<ToolboxAttributedTextContext *>(installed);
    controller.renderContext = context;
    loka::core::testing::failLokaAllocRaw("ToolboxAttributedText", "Break", 0);
    toolbox_host::reset();
    context->layout(&controller, seat);
    const ToolboxAttributedTextTable &table = ToolboxAttributedTextContextAccess::table(*context);
    LOKA_VERIFY(table.valid());
    LOKA_VERIFY(table.lines().lineCount() == 2);
    LOKA_VERIFY(table.lines().line(1).fragmentCount == 2);
    LOKA_VERIFY(table.height() == 46);
    LOKA_VERIFY(ToolboxAttributedTextContextAccess::rect(*context).bottom == 66);
    Pin("ToolboxAttributedTextLargestSpanBusy");
    LOKA_VERIFY(controller.cursor.entries == 1);
    LOKA_VERIFY(controller.cursor.depth == 0);
    Pin("ToolboxAttributedTextLinearNativeMeasurement");
    LOKA_VERIFY(toolbox_host::metrics == 2);
    LOKA_VERIFY(toolbox_host::fonts == 3);
    LOKA_VERIFY(toolbox_host::measures == 2);
    LOKA_VERIFY(toolbox_host::widths == 0);
    std::printf("measure: font selections=%d TextWidth=%d MeasureText=%d GetFontInfo=%d scope acquisitions=1\n",
                toolbox_host::fonts,
                toolbox_host::widths,
                toolbox_host::measures,
                toolbox_host::metrics);
    Pin("ToolboxAttributedTextTransparentCompositionAndEraseOnReplay");
    context->render(&controller);
    LOKA_VERIFY(toolbox_host::erases == 0);
    toolbox_host::draws.clear();
    Repaint(controller, *context);
    Pin("ToolboxAttributedTextDrawsUnderCallerClipWhenRegionAllocationFails");
    {
      // Classic memory pressure: NewRgn refuses, the clip is inactive, and the
      // text must still draw under the caller's clip (Text's fallback) while
      // the presented history stays unknown.
      const std::size_t before = toolbox_host::draws.size();
      const int erasesBefore = toolbox_host::erases;
      toolbox_host::failRegions = 2;
      context->render(&controller);
      LOKA_VERIFY(toolbox_host::draws.size() == before + 3);
      LOKA_VERIFY(!ToolboxAttributedTextContextAccess::known(*context));
      toolbox_host::failRegions = 0;
      toolbox_host::draws.clear();
      Repaint(controller, *context);
      toolbox_host::erases = erasesBefore;
    }
    Pin("ToolboxAttributedTextSpanFacesAndBaseline");
    LOKA_VERIFY(toolbox_host::draws.size() == 3);
    LOKA_VERIFY(toolbox_host::draws[0].face == bold);
    LOKA_VERIFY(toolbox_host::draws[1].face == bold);
    LOKA_VERIFY(toolbox_host::draws[2].face == italic);
    LOKA_VERIFY(toolbox_host::draws[0].y == 32);
    LOKA_VERIFY(toolbox_host::draws[1].y == 61);
    LOKA_VERIFY(toolbox_host::draws[2].y == 61);
    LOKA_VERIFY(toolbox_host::draws[2].x == 20);
    LOKA_VERIFY(ToolboxAttributedTextContextAccess::known(*context));
    LOKA_VERIFY(window.port.txSize == 12 && window.port.txFace == 0);
    const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
    LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);

    Pin("ToolboxAttributedTextWidthInvalidates");
    seat = Seat(60);
    context->layout(&controller, seat);
    LOKA_VERIFY(table.lines().lineCount() == 1);
    LOKA_VERIFY(!ToolboxAttributedTextContextAccess::known(*context));
    Repaint(controller, *context);
    LOKA_VERIFY(toolbox_host::erases == 2);

    Pin("ToolboxAttributedTextFailedRebuildDegrades");
    loka::core::testing::failLokaAllocRaw("ToolboxAttributedText", "Break", 1);
    seat = Seat(30);
    context->layout(&controller, seat);
    LOKA_VERIFY(!table.valid());
    LOKA_VERIFY(!ToolboxAttributedTextContextAccess::known(*context));
    LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_REFUSED);
    context->render(&controller);
    LOKA_VERIFY(toolbox_host::erases == 2);
    seat = Seat(30);
    context->layout(&controller, seat);
    LOKA_VERIFY(table.valid());

    Pin("ToolboxAttributedTextHiddenUpdateAndPartialPaint");
    SetRect(&controller.projectionClip, 200, 200, 220, 220);
    node.props = AttributedTextProps(updated);
    context->onPropsApplied();
    seat = Seat(80);
    context->layout(&controller, seat);
    LOKA_VERIFY(table.valid());
    Rect hiddenPaint = ToolboxAttributedTextContextAccess::paintRect(*context);
    LOKA_VERIFY(EmptyRect(&hiddenPaint));
    context->render(&controller);
    LOKA_VERIFY(!ToolboxAttributedTextContextAccess::known(*context));
    SetRect(&controller.projectionClip, -30000, -30000, 30000, 30000);
    seat = Seat(80);
    context->layout(&controller, seat);
    RgnHandle savedClip = NewRgn();
    RgnHandle partial = NewRgn();
    GetClip(savedClip);
    const Rect strip = {20, 10, 25, 25};
    RectRgn(partial, &strip);
    SetClip(partial);
    context->render(&controller);
    LOKA_VERIFY(!ToolboxAttributedTextContextAccess::known(*context));
    SetClip(savedClip);
    DisposeRgn(partial);
    DisposeRgn(savedClip);
    const int beforeUpdateErase = toolbox_host::erases;
    Repaint(controller, *context);
    LOKA_VERIFY(toolbox_host::erases == beforeUpdateErase + 1);
    LOKA_VERIFY(ToolboxAttributedTextContextAccess::known(*context));

    Pin("ToolboxAttributedTextRetainedAndRetired");
    context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
    LOKA_VERIFY(controller.compositionReplay.required());
    LOKA_VERIFY(table.valid());
    LOKA_VERIFY(!ToolboxAttributedTextContextAccess::known(*context));
    context->onFactChanged(NODE_FACT_DETACHED_RETAINED, NODE_FACT_ATTACHED);
    seat = Seat(30);
    context->layout(&controller, seat);
    context->render(&controller);
    LOKA_VERIFY(ToolboxAttributedTextContextAccess::known(*context));
    controller.renderContext = 0;
    context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_RETIRED);
    LOKA_VERIFY(!controller.compositionReplay.required());
    LOKA_VERIFY(!table.valid());
    delete &node; // Node retirement must clear the table before the context destructor asserts.
    LOKA_VERIFY(controller.retired.size() == 1);
    LOKA_VERIFY(!controller.compositionReplay.required());
    controller.retired.clear();
    loka::core::testing::allowLokaAllocRaw();
  }
  Pin("ToolboxAttributedTextUnsetUsesOriginalPort");
  window.port.txFace = italic;
  const AttributedString inherited = Styled("a", Bold + TextStyle().italic(false)) + Styled("b", TextStyle());
  {
    ToolboxAttributedTextTable projected;
    LOKA_VERIFY(projected.build(inherited, BlockStyle(), 100, controller));
    toolbox_host::reset();
    projected.draw(0, 0, controller, BlockStyle(), 0);
    LOKA_VERIFY(toolbox_host::draws.size() == 2);
    LOKA_VERIFY(toolbox_host::draws[0].face == bold);
    LOKA_VERIFY(toolbox_host::draws[1].face == italic);
    LOKA_VERIFY(window.port.txFace == italic);
  }
  Pin("ToolboxAttributedTextLongJoinedBufferAndUtf8Ranges");
  window.port.txFace = 0;
  {
    ToolboxAttributedTextTable projected;
    const AttributedString longText = Styled(loka::core::String(std::string(160, 'a')), Bold)
                                      + Styled(loka::core::String(std::string(160, 'b')), Bold);
    LOKA_VERIFY(projected.build(longText, BlockStyle(), 0, controller));
    toolbox_host::reset();
    projected.draw(0, 0, controller, BlockStyle(), 0);
    LOKA_VERIFY(toolbox_host::draws.size() == 1);
    LOKA_VERIFY(toolbox_host::draws[0].length == 320);
    LOKA_VERIFY(toolbox_host::draws[0].bytes == std::string(160, 'a') + std::string(160, 'b'));
    const AttributedString unicode = Styled("\xC3\xA9", Bold) + Styled("\xE3\x81\x82", Italic);
    LOKA_VERIFY(projected.build(unicode, BlockStyle().wrap(TEXT_WRAP_CHAR), 10, controller));
    LOKA_VERIFY(projected.lines().lineCount() == 2);
    toolbox_host::reset();
    projected.draw(0, 0, controller, BlockStyle(), 0);
    LOKA_VERIFY(toolbox_host::draws.size() == 2);
    LOKA_VERIFY(toolbox_host::draws[0].bytes == "\xC3\xA9");
    LOKA_VERIFY(toolbox_host::draws[1].bytes == "\xE3\x81\x82");
    LOKA_VERIFY(projected.build(unicode, BlockStyle().wrap(TEXT_WRAP_NONE), 10, controller));
    LOKA_VERIFY(projected.lines().lineCount() == 1);
  }

  Pin("ToolboxAttributedTextLongWordLookaheadAndEllipsis");
  {
    ToolboxAttributedTextTable projected;
    const AttributedString word = Styled(loka::core::String(std::string("a ") + std::string(9000, 'x')), TextStyle());
    LOKA_VERIFY(projected.build(word, BlockStyle().wrap(TEXT_WRAP_WORD), 40, controller));
    LOKA_VERIFY(projected.lines().lineCount() > 800);
    const AttributedString mixed = Styled("ab", Bold) + Styled("cdefgh", Italic);
    const BlockStyle ellipsis = BlockStyle().truncation(TEXT_TRUNCATION_ELLIPSIS);
    LOKA_VERIFY(projected.build(mixed, ellipsis, 25, controller));
    toolbox_host::reset();
    LOKA_VERIFY(projected.draw(0, 0, controller, ellipsis, 25));
    LOKA_VERIFY(toolbox_host::draws.size() == 2);
    LOKA_VERIFY(toolbox_host::draws[0].bytes == "ab");
    LOKA_VERIFY(toolbox_host::draws[1].bytes == "...");
    LOKA_VERIFY(toolbox_host::draws[1].face == italic);
    LOKA_VERIFY(toolbox_host::draws[1].x == 10);
  }
  Pin("ToolboxAttributedTextDistinctResolvedMetricsAndEmptyLines");
  loka::core::testing::failLokaAllocRaw("TextLineBreaker", "Table", 0);
  {
    ToolboxAttributedTextTable projected;
    const AttributedString equivalent = Styled("a", TextStyle()) + Styled("b", TextStyle().italic(false));
    toolbox_host::reset();
    LOKA_VERIFY(projected.build(equivalent, BlockStyle(), 40, controller));
    LOKA_VERIFY(toolbox_host::fonts == 3);
    LOKA_VERIFY(toolbox_host::measures == 2);
    LOKA_VERIFY(toolbox_host::metrics == 1);
    toolbox_host::reset();
    LOKA_VERIFY(projected.build(AttributedString(), BlockStyle(), 40, controller));
    LOKA_VERIFY(projected.height() == 17);
    LOKA_VERIFY(toolbox_host::metrics == 1);
    const AttributedString newline = Styled("x\n", FontSize<24>());
    LOKA_VERIFY(projected.build(newline, BlockStyle(), 40, controller));
    LOKA_VERIFY(projected.lines().lineCount() == 2);
    LOKA_VERIFY(projected.height() == 58);
    const AttributedString longText = Styled(loka::core::String(std::string(100, 'x')), Bold);
    LOKA_VERIFY(projected.build(longText, BlockStyle(), 40, controller));
    loka::core::testing::failLokaAllocRaw("TextLineBreaker", "Table", 3);
    LOKA_VERIFY(!projected.build(longText, BlockStyle(), 40, controller));
    LOKA_VERIFY(!projected.valid());
  }
  loka::core::testing::allowLokaAllocRaw();
  Pin("ToolboxAttributedTextCompositionReplayMembership");
  {
    ToolboxCompositionReplay ledger;
    ToolboxCompositionReplay::Registration first, second;
    LOKA_VERIFY(!ledger.required());
    first.attach(ledger);
    second.attach(ledger);
    first.attach(ledger); // Idempotent reattach, no duplicate edge.
    first.clear(); // Non-head removal must leave the other demand registered.
    LOKA_VERIFY(ledger.required());
    second.clear();
    LOKA_VERIFY(!ledger.required());
    { ToolboxCompositionReplay::Registration scoped; scoped.attach(ledger); }
    LOKA_VERIFY(!ledger.required());
  }
  std::printf("sizeof table=%lu context=%lu\n",
              static_cast<unsigned long>(sizeof(ToolboxAttributedTextTable)),
              static_cast<unsigned long>(sizeof(ToolboxAttributedTextContext)));
  std::puts("Toolbox AttributedText host pins passed (QuickDraw substitute; no native pixel claim)");
  return 0;
}
