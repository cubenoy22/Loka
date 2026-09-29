#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxPaintSupport.hpp"
#include "app/nodes/AttributedText.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/projection/CollectPaintAnswers.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "debug/ToolboxSceneDebugStats.hpp"
#include "support/TestVerify.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include <cstdio>

namespace
{
#include "ToolboxPaintAnswers.hpp"
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  enum
  {
    kBoardCells = 9
  };
  const char *const kIds[kBoardCells] = {"c0", "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8"};
  // The fold must not depend on whether the status is enumerated before or after the board.
  bool gStatusLast = false;
  class FoldNode;
  typedef BoundaryPropsFor<FoldNode> FoldProps;
  /** MINES.JS in miniature: one Boundary owns a status and a board whose cells
      all change in one apply (a flood fill). */
  class FoldNode : public StdCompositionBoundaryNodeBase<FoldProps>
  {
  public:
    typedef FoldProps::TypeTag TypeTag;
    explicit FoldNode(const FoldProps &p) : StdCompositionBoundaryNodeBase<FoldProps>(p)
    {
      this->state(this->status, String::Literal("Mines left: 10"));
      for (int i = 0; i < kBoardCells; ++i)
        this->state(this->cells[i], String::Literal("."));
    }
    virtual void composeNode(NodeComposition &c)
    {
      Row board;
      for (int i = 0; i < kBoardCells; ++i)
        board << Cell(this->cells[i].state()).TEST_ID(kIds[i]);
      if (gStatusLast)
        c.declare(Column() << (board << Cell("unchanged").TEST_ID("neighbour")).TEST_ID("board") << Cell(this->status.state()).TEST_ID("status"));
      else
        c.declare(Column() << Cell(this->status.state()).TEST_ID("status") << (board << Cell("unchanged").TEST_ID("neighbour")).TEST_ID("board"));
    }
    NodeState<String> status;
    NodeState<String> cells[kBoardCells];
  };
  LayoutState Seat(short x, short y, short width)
  {
    LayoutState s;
    s.x = x; s.y = y; s.width = width; s.height = 20; s.spacing = 0;
    return s;
  }
  ToolboxCellContext *Attach(Scene &scene, ToolboxScenePlatformController &controller, const char *id,
                             const LayoutState &seat)
  {
    Node *node = 0;
    loka::dsl::FlowError error;
    LOKA_VERIFY(loka::dsl::testing::LookupNodeById<Node>(&scene, id, node, error));
    ToolboxCellContext *context = new ToolboxCellContext(node->asCellNode(), &controller);
    node->setContext(context);
    LayoutState s = seat;
    context->layout(&controller, s);
    context->draw(&controller);
    return context;
  }
}
void Run(bool statusLast, bool partialNeighbour = false)
{
  gStatusLast = statusLast;
  using loka::dsl::testing::SceneTestAccess;
  ToolboxWindow window;
  ToolboxScenePlatformController controller(&window);
  Scene scene((Boundary<FoldNode>(FoldProps())));
  scene.mount(&controller);
  SceneTestAccess::updateAttached(scene, true);
  FoldNode *root = static_cast<FoldNode *>(SceneTestAccess::rootBoundary(scene));
  ToolboxCellContext *residents[kBoardCells + 2];
  Rect rects[kBoardCells + 2];
  residents[0] = Attach(scene, controller, "status", Seat(10, 0, 120));
  SetRect(&rects[0], 10, 0, 130, 20);
  for (int i = 0; i < kBoardCells; ++i)
  {
    const short x = static_cast<short>(partialNeighbour && i >= 2 ? 100 + 100 * i : 10 + 20 * i);
    const short y = partialNeighbour && i == 1 ? 50 : 40;
    residents[i + 1] = Attach(scene, controller, kIds[i], Seat(x, y, 20));
    SetRect(&rects[i + 1], x, y, x + 20, y + 20);
  }
  // In the partial case c0 and c1 have the cheapest bounding union:
  // (10,40)-(50,70). The neighbour touches neither changed cell, but its
  // top five pixels intersect that union's otherwise empty lower-left corner.
  const short neighbourX = partialNeighbour ? 10 : 220;
  const short neighbourY = partialNeighbour ? 65 : 40;
  residents[kBoardCells + 1] = Attach(scene, controller, "neighbour", Seat(neighbourX, neighbourY, 20));
  SetRect(&rects[kBoardCells + 1], neighbourX, neighbourY, neighbourX + 20, neighbourY + 20);
  {
    StateTrackerGuard guard(root->tracker());
    root->status.set(String::Literal("Mines left: 9"));
    for (int i = 0; i < kBoardCells; ++i)
      root->cells[i].set(String::Literal("1"));
  }
  const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
  ToolboxSceneDebugStats stats;
  ToolboxPaintAnswerSource source(stats);
  PaintAnswerBuffer<> answers;
  BoundaryLocalApplyInfo info;
  info.paintKind = LOCAL_APPLY_PAINT_GENERIC;
  const PaintApplyVerdict verdict = CollectPaintAnswers(*root, query, answers, source);
  std::printf("fold collector (status %s): exact=%u refused=%u widened=%d rects=%u\n", gStatusLast ? "last" : "first", verdict.exactCount(),
              verdict.refusedCount(), verdict.widened() ? 1 : 0, answers.count());
  std::fflush(stdout);
  // Ten changed Cells in one apply: the rail must not widen to the window.
  LOKA_VERIFY(verdict.canSkipBroadPaint(info));
  LOKA_VERIFY(answers.count() <= kApplyPaintPlanCapacity);
  bool covered[kBoardCells + 1] = {false};
  for (unsigned r = 0; r < answers.count(); ++r)
  {
    const PaintDamage &d = answers.entry(r).damage;
    if (d.y < 20)
      LOKA_VERIFY(d.x == 10 && d.y == 0 && d.width == 120 && d.height == 20);
    for (int i = 0; i <= kBoardCells; ++i)
      if (d.x <= rects[i].left && d.y <= rects[i].top
          && d.x + d.width >= rects[i].right && d.y + d.height >= rects[i].bottom)
        covered[i] = true;
  }
  for (int i = 0; i <= kBoardCells; ++i)
    LOKA_VERIFY(covered[i]);
  const PaintAnswer neighbourBefore = residents[kBoardCells + 1]->queryPaintDamage(query);
  LOKA_VERIFY(neighbourBefore.kind == PAINT_ANSWER_EXACT);
  LOKA_VERIFY(neighbourBefore.damage.width == 0 || neighbourBefore.damage.height == 0);
  unsigned partialDraws = 0;
  for (unsigned r = 0; r < answers.count(); ++r)
  {
    const PaintDamage &d = answers.entry(r).damage;
    Rect delivery;
    SetRect(&delivery, d.x, d.y, d.x + d.width, d.y + d.height);
    ToolboxPaintClip clip(delivery);
    for (int i = 0; i < kBoardCells + 2; ++i)
    {
      if (delivery.left < rects[i].right && delivery.right > rects[i].left
          && delivery.top < rects[i].bottom && delivery.bottom > rects[i].top)
      {
        residents[i]->draw(&controller);
        if (partialNeighbour && i == kBoardCells + 1)
        {
          LOKA_VERIFY(delivery.top > rects[i].top || delivery.left > rects[i].left
                      || delivery.bottom < rects[i].bottom || delivery.right < rects[i].right);
          ++partialDraws;
          const PaintAnswer after = residents[i]->queryPaintDamage(query);
          std::printf("partial neighbour: clip=(%d,%d,%d,%d) resident=(%d,%d,%d,%d) kind=%d reason=%d\n",
                      delivery.left, delivery.top, delivery.right, delivery.bottom,
                      rects[i].left, rects[i].top, rects[i].right, rects[i].bottom,
                      after.kind, after.reason);
        }
      }
    }
  }
  if (partialNeighbour)
    LOKA_VERIFY(partialDraws != 0);
  {
    StateTrackerGuard guard(root->tracker());
    root->cells[0].set(String::Literal("2"));
  }
  const PaintApplyVerdict next = CollectPaintAnswers(*root, query, answers, source);
  std::printf("next click (status %s, partial neighbour %d): exact=%u refused=%u widened=%d reason=%d\n",
              statusLast ? "last" : "first", partialNeighbour ? 1 : 0,
              next.exactCount(), next.refusedCount(), next.widened() ? 1 : 0, next.refusalReason());
  std::fflush(stdout);
  LOKA_VERIFY(next.canSkipBroadPaint(info));
  LOKA_VERIFY(next.refusedCount() == 0);
  SceneTestAccess::unmount(scene);
}
int main()
{
  Run(false);
  Run(true);
  Run(false, true);
  Run(true, true);
  std::puts("Paint fold pins passed");
}
