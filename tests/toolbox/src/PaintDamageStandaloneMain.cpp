#include <cstdio>
#include <cstring>

#include "StandaloneFlowRunner.hpp"
#include "ObservedMainDefinition.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "app/RectSurface.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "app/scene/projection/CollectPaintAnswers.hpp"
#include "app/scene/projection/NativeNodeContext.hpp"
#include "context/ToolboxPaintSupport.hpp"
#include "core/io/File.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileIO.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;

  template <bool HasViewport> class PaintDamageNode;

  /** The Boundary owns both models and the live text for the whole pin. */
  template <bool HasViewport>
  class PaintDamageNode : public StdCompositionBoundaryNodeBase<BoundaryPropsFor<PaintDamageNode<HasViewport> > >
  {
  public:
    typedef BoundaryPropsFor<PaintDamageNode<HasViewport> > Props;
    typedef typename Props::TypeTag TypeTag;
    explicit PaintDamageNode(const Props &props)
        : StdCompositionBoundaryNodeBase<Props>(props)
    {
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(4, 4, 12, 12);
      this->state(this->first_, model);
      this->state(this->sibling_, model);
      this->state(this->text_, loka::core::String::Literal("Viewport paint damage"));
    }
    virtual void composeNode(NodeComposition &composition)
    {
      Column surfaces = Column()
                        << RectSurface(this->first_.state()).size(180, 50).useRegionClip(true).TEST_ID("PaintDamage.First")
                        << RectSurface(this->sibling_.state()).size(180, 50).TEST_ID("PaintDamage.Sibling");
      if (HasViewport)
        composition.declare(Box().size(240, 180) << (ScrollView() << (surfaces << Text(this->text_.state()))));
      else
        composition.declare(surfaces);
    }
    void advance()
    {
      RectSurfaceModel model = this->first_.get();
      model.rects[0].x += 12;
      loka::core::StateTrackerGuard guard(this->tracker());
      this->first_.set(model);
    }

  private:
    NodeState<RectSurfaceModel> first_;
    NodeState<RectSurfaceModel> sibling_;
    NodeState<loka::core::String> text_;
  };

  typedef PaintDamageNode<true> ViewportDamageNode;
  typedef PaintDamageNode<false> PlainDamageNode;

  class CompositedDamageNode;
  typedef BoundaryPropsFor<CompositedDamageNode> CompositedDamageProps;
  class CompositedDamageNode : public StdCompositionBoundaryNodeBase<CompositedDamageProps>
  {
  public:
    typedef CompositedDamageProps::TypeTag TypeTag;
    explicit CompositedDamageNode(const CompositedDamageProps &props)
        : StdCompositionBoundaryNodeBase<CompositedDamageProps>(props)
    {
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(4, 4, 12, 12);
      this->state(this->model_, model);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Box().size(180, 110)
                          << (ScrollView()
                              << Text("Baseline").TEST_ID("PaintDamage.Baseline")
                              << (ZStack() << RectSurface(this->model_.state()).size(150, 70) << Text("ZStack"))));
    }
    void advance()
    {
      RectSurfaceModel model = this->model_.get();
      model.rects[0].x += 12;
      loka::core::StateTrackerGuard guard(this->tracker());
      this->model_.set(model);
    }
  private:
    NodeState<RectSurfaceModel> model_;
  };

  class EditDamageNode;
  typedef BoundaryPropsFor<EditDamageNode> EditDamageProps;
  class EditDamageNode : public StdCompositionBoundaryNodeBase<EditDamageProps>
  {
  public:
    typedef EditDamageProps::TypeTag TypeTag;
    explicit EditDamageNode(const EditDamageProps &props)
        : StdCompositionBoundaryNodeBase<EditDamageProps>(props)
    {
      this->state(this->text_, loka::core::String::Literal("Clipped edit"));
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Box().size(180, 50)
                          << (ScrollView()
                              << (Column() << Box().size(150, 48)
                                  << EditText(this->text_).TEST_ID("PaintDamage.Edit"))));
    }
  private:
    NodeState<loka::core::String> text_;
  };

  struct FindEdit : public IPaintResidentVisitor
  {
    FindEdit() : context(0) {}
    virtual void visit(Node *node, NodeContext *native, BoundaryNode *)
    {
      if (node->testId() == "PaintDamage.Edit")
        this->context = static_cast<ToolboxEditTextContext *>(native);
    }
    ToolboxEditTextContext *context;
  };

  /** This fixture installs only the two concrete production drawer kinds.
      Recollecting their read-only answers observes the gate without adding a
      production telemetry door or retaining the collector's resident borrows. */
  struct ProbeSource
  {
    ProbeSource() : siblingX(0), siblingY(0), foundSibling(false), first(0), firstDamage() {}
    bool queryPaintAnswer(Node *node, NodeContext *context, const PaintQuery &query, PaintAnswer &answer)
    {
      if (node->kind() != NODE_KIND_RECT_SURFACE && node->kind() != NODE_KIND_TEXT)
        return false;
      answer = context ? static_cast<NativeNodeContext *>(context)->queryPaintDamage(query)
                       : PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
      if (node->testId() == "PaintDamage.First" && answer.kind == PAINT_ANSWER_EXACT)
      {
        this->first = context;
        this->firstDamage = answer.damage;
      }
      if (node->testId() == "PaintDamage.Sibling" && answer.kind == PAINT_ANSWER_EXACT)
      {
        this->siblingX = answer.damage.x + 8;
        this->siblingY = answer.damage.y + 8;
        this->foundSibling = true;
      }
      return true;
    }
    int siblingX, siblingY;
    bool foundSibling;
    NodeContext *first;
    PaintDamage firstDamage;
  };

  /** Window borrows remain inside the configuration that owns this finite run. */
  class ObservedWindowDefinition : public WindowDefinition<WindowProps>
  {
  public:
    ObservedWindowDefinition(const WindowProps &props, Window **result)
        : WindowDefinition<WindowProps>(props), result_(result) {}
    virtual WindowDefinitionBase *clone() const
    {
      return new (std::nothrow) ObservedWindowDefinition(*this);
    }
    virtual Window *create(PlatformContext *context) const
    {
      Window *window = WindowDefinition<WindowProps>::create(context);
      *this->result_ = window;
      return window;
    }
  private:
    Window **result_;
  };

  class PaintDamageConfig : public AppConfigurable
  {
  public:
    explicit PaintDamageConfig(PlatformContext *context)
        : AppConfigurable(context), app_(0), node_(0), composited_(0), edit_(0), log_(0), phase_(SETTLE), result_(0),
          initial_(), marker_(), gate_(false), editGeometry_(), paintWindow_(0), compositedWindow_(0), editWindow_(0), plain_(0), plainWindow_(0)
    {
      if (loka::platform::file::ResolveApplicationSidecar(
              loka::file::File::Application() << loka::file::File("LOG.TXT"), this->file_))
        this->log_ = loka::platform::file::OpenWriteTruncate(this->file_);
      if (!this->log_)
        this->result_ = 1;
      else if (std::fprintf(this->log_, "paint-damage BEGIN\r") < 0
               || !loka::platform::file::FlushWrite(this->log_, this->file_))
        this->result_ = 1;
    }
    virtual ~PaintDamageConfig()
    {
      if (this->log_)
        std::fclose(this->log_);
    }
    void setApp(App *app) { this->app_ = app; }
    int exitCode() const { return this->result_; }
    virtual void compose(AppComposition &composition)
    {
      composition << ObservedWindowDefinition(WindowProps().frame(50, 50, 280, 220).title("Paint damage")
                               .scene(loka::scenario_tests::ObservedMainDefinition<ViewportDamageNode::Props, ViewportDamageNode>(
                                   ViewportDamageNode::Props(), &this->node_))
                               .visible(true).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->paintWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(50, 285, 280, 165).title("Exact damage")
                               .scene(loka::scenario_tests::ObservedMainDefinition<PlainDamageNode::Props, PlainDamageNode>(
                                   PlainDamageNode::Props(), &this->plain_))
                               .visible(true).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->plainWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 50, 220, 160).title("Composited damage")
                               .scene(loka::scenario_tests::ObservedMainDefinition<CompositedDamageProps, CompositedDamageNode>(
                                   CompositedDamageProps(), &this->composited_))
                               .visible(true).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->compositedWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 160).title("Edit replay")
                               .scene(loka::scenario_tests::ObservedMainDefinition<EditDamageProps, EditDamageNode>(
                                   EditDamageProps(), &this->edit_))
                               .visible(true).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->editWindow_);
    }

  private:
    enum Phase { SETTLE, WRITE, CHECK, PLAIN_WRITE, PLAIN_CHECK, INVALIDATED_WRITE, INVALIDATED_CHECK,
                 COMPOSITED_WRITE, COMPOSITED_CHECK, EDIT_WRITE, EDIT_CHECK, COMPLETE };
    App *app_;
    ViewportDamageNode *node_;
    CompositedDamageNode *composited_;
    EditDamageNode *edit_;
    loka::platform::file::FileHandle file_;
    std::FILE *log_;
    Phase phase_;
    int result_;
    ToolboxSceneDebugStats initial_;
    Point marker_;
    bool gate_;
    ToolboxScenePlatformController::EditTextGeometry editGeometry_;

    Window *paintWindow_;
    Window *compositedWindow_;
    Window *editWindow_;
    PlainDamageNode *plain_;
    Window *plainWindow_;

    void recordArm(const char *name, bool pass, Phase next)
    {
      if (!pass)
        this->result_ = 1;
      if (std::fprintf(this->log_, "%s %s\r", name, pass ? "PASS" : "FAIL") < 0
          || !loka::platform::file::FlushWrite(this->log_, this->file_))
        this->result_ = 1;
      this->phase_ = next;
    }

    /** Only the active Window receives idle. Any active window drives the
        same finite sequence over the four explicit fixture borrows. */
    static void DispatchIdle(Window *, double elapsed, void *data)
    {
      PaintDamageConfig *self = static_cast<PaintDamageConfig *>(data);
      Window *target = 0;
      switch (self->phase_)
      {
      case SETTLE: case WRITE: case CHECK:
        target = self->paintWindow_;
        break;
      case PLAIN_WRITE: case PLAIN_CHECK: case INVALIDATED_WRITE: case INVALIDATED_CHECK:
        target = self->plainWindow_;
        break;
      case COMPOSITED_WRITE: case COMPOSITED_CHECK:
        target = self->compositedWindow_;
        break;
      case EDIT_WRITE: case EDIT_CHECK:
        target = self->editWindow_;
        break;
      case COMPLETE:
        return;
      }
      ToolboxWindow *native = target ? target->asToolboxWindow() : 0;
      if (!native)
      {
        self->finish(false);
        return;
      }
      // One bounded delivery per phase; never wait for a permanently pending
      // flag. Native drawing may itself publish work for the following phase.
      target->flushSceneInvalidation();
      native->flushInvalidate();
      if (target == self->paintWindow_)
        OnPaintIdle(target, self->node_, self);
      else if (target == self->plainWindow_)
        OnPaintIdle(target, self->plain_, self);
      else if (target == self->compositedWindow_)
        OnCompositedIdle(target, elapsed, data);
      else
        OnEditIdle(target, elapsed, data);
    }

    void finish(bool pass)
    {
      this->phase_ = COMPLETE;
      pass = pass && this->result_ == 0;
      this->result_ = pass ? 0 : 1;
      if (std::fprintf(this->log_, "paint-damage %s\r", pass ? "PASS" : "FAIL") < 0
          || !loka::platform::file::FlushWrite(this->log_, this->file_))
        this->result_ = 1;
      this->app_->quit();
    }
    template <bool HasViewport>
    static void OnPaintIdle(Window *window, PaintDamageNode<HasViewport> *node, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window ? window->asToolboxWindow() : 0;
      if (!native || !window->scene() || !node)
      {
        self->finish(false);
        return;
      }
      ToolboxScenePlatformController *controller = static_cast<ToolboxScenePlatformController *>(
          loka::dsl::testing::SceneTestAccess::platformController(*window->scene()));
      if (!controller)
      {
        self->finish(false);
        return;
      }
      if (self->phase_ == SETTLE)
      {
        self->phase_ = WRITE;
        return;
      }
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      if (self->phase_ == INVALIDATED_WRITE)
      {
        const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
        PaintAnswerBuffer<> answers;
        ProbeSource source;
        CollectPaintAnswers(*node, query, answers, source);
        if (!source.first)
        {
          SetPort(previousPort);
          self->recordArm("unknown-history-setup", false, COMPOSITED_WRITE);
          return;
        }
        self->marker_.h = static_cast<short>(source.firstDamage.x + 18);
        self->marker_.v = static_cast<short>(source.firstDamage.y + 8);
        const bool oldPixelBlack = GetPixel(self->marker_.h, self->marker_.v) != 0;
        self->initial_ = controller->debugStatsForTesting();
        node->advance();
        // Freeze the real exact request before invalidating presentation
        // history through the retained-props lifecycle door. The pending
        // window delivery must reconstruct all ground with region clipping on.
        loka::dsl::testing::SceneTestAccess::flushInvalidation(*window->scene());
        source.first->onPropsApplied();
        self->phase_ = INVALIDATED_CHECK;
        SetPort(previousPort);
        if (!oldPixelBlack)
          self->recordArm("unknown-history-setup", false, COMPOSITED_WRITE);
        return;
      }
      if (self->phase_ == INVALIDATED_CHECK)
      {
        const bool erased = GetPixel(self->marker_.h, self->marker_.v) == 0;
        const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
        const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
        const int dirty = stats.windowFlushDirtyCount - self->initial_.windowFlushDirtyCount;
        std::fprintf(self->log_, "unknown_history_old_pixel_erased=%d whole_window=%d dirty_flushes=%d\r",
                     erased ? 1 : 0, whole, dirty);
        SetPort(previousPort);
        self->recordArm("unknown-history", erased && whole == 0 && dirty > 0, COMPOSITED_WRITE);
        return;
      }
      if (self->phase_ == WRITE || self->phase_ == PLAIN_WRITE)
      {
        const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
        PaintAnswerBuffer<> answers;
        ProbeSource source;
        const PaintApplyVerdict verdict = CollectPaintAnswers(*node, query, answers, source);
        BoundaryLocalApplyInfo info;
        info.paintKind = LOCAL_APPLY_PAINT_GENERIC;
        self->gate_ = verdict.canSkipBroadPaint(info);
        if (!source.foundSibling)
        {
          SetPort(previousPort);
          self->recordArm(HasViewport ? "viewport-broad-fallback-setup" : "plain-exact-setup", false,
                          HasViewport ? PLAIN_WRITE : INVALIDATED_WRITE);
          return;
        }
        self->marker_.h = static_cast<short>(source.siblingX);
        self->marker_.v = static_cast<short>(source.siblingY);
        // Sample actual sibling ink: a correct full repaint preserves it.
        // An artificial pixel in white ground would reject that valid repaint.
        if (!GetPixel(self->marker_.h, self->marker_.v))
        {
          SetPort(previousPort);
          self->recordArm(HasViewport ? "viewport-broad-fallback-setup" : "plain-exact-setup", false,
                          HasViewport ? PLAIN_WRITE : INVALIDATED_WRITE);
          return;
        }
        self->initial_ = controller->debugStatsForTesting();
        node->advance();
        self->phase_ = HasViewport ? CHECK : PLAIN_CHECK;
      }
      else
      {
        const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
        const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
        const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
        const int draws = stats.totalControlDrawCount - self->initial_.totalControlDrawCount;
        const bool siblingPreserved = GetPixel(self->marker_.h, self->marker_.v) != 0;
        std::fprintf(self->log_, "gate=%d invalidate_rects=%d whole_window=%d control_draws=%d sibling_preserved=%d\r",
                     self->gate_ ? 1 : 0, rects, whole, draws, siblingPreserved ? 1 : 0);
        SetPort(previousPort);
        const bool viewportReason = stats.windowFullRequestSource
                                    && std::strcmp(stats.windowFullRequestSource, "paint-widened-viewport-render") == 0;
        std::fprintf(self->log_, "delivery=%s reason=%s\r", HasViewport ? "WIDENED" : "EXACT",
                     HasViewport && stats.windowFullRequestSource ? stats.windowFullRequestSource : "none");
        self->recordArm(HasViewport ? "viewport-broad-fallback" : "plain-exact",
                        self->gate_ && siblingPreserved
                        && (HasViewport ? whole == 1 && viewportReason
                                        : rects == 1 && whole == 0),
                        HasViewport ? PLAIN_WRITE : INVALIDATED_WRITE);
        return;
      }
      SetPort(previousPort);
    }
    static void OnCompositedIdle(Window *window, double, void *data)
    {
      PaintDamageConfig *self = static_cast<PaintDamageConfig *>(data);
      if (self->phase_ != COMPOSITED_WRITE && self->phase_ != COMPOSITED_CHECK)
        return;
      ToolboxWindow *native = window ? window->asToolboxWindow() : 0;
      if (!native || !window->scene() || !self->composited_)
      {
        self->finish(false);
        return;
      }
      ToolboxScenePlatformController *controller = static_cast<ToolboxScenePlatformController *>(
          loka::dsl::testing::SceneTestAccess::platformController(*window->scene()));
      if (!controller)
      {
        self->finish(false);
        return;
      }
      if (self->phase_ == COMPOSITED_WRITE)
      {
        // This fixture's viewport starts at the root seat y = 24. Require a
        // complete recorded Text placement and visible ink inside that seat.
        TextNode *text = 0;
        loka::dsl::FlowError error;
        loka::dsl::testing::LookupNodeById<TextNode>(
            window->scene(), "PaintDamage.Baseline", text, error);
        const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
        const PaintAnswer answer = text && text->getContext()
            ? static_cast<NativeNodeContext *>(text->getContext())->queryPaintDamage(query)
            : PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
        const bool placedInside = answer.kind == PAINT_ANSWER_EXACT && answer.damage.y >= 24;
        GrafPtr previousPort;
        GetPort(&previousPort);
        SetPort(native->window());
        bool above = false;
        bool inside = false;
        for (short y = 12; y < 38; ++y)
          for (short x = 12; x < 70; ++x)
            if (GetPixel(x, y))
            {
              if (y < 24)
                above = true;
              else
                inside = true;
            }
        SetPort(previousPort);
        self->recordArm("scrollview-first-text-placement", placedInside, COMPOSITED_WRITE);
        self->recordArm("startup-text-baseline", !above && inside, COMPOSITED_WRITE);
        self->initial_ = controller->debugStatsForTesting();
        self->composited_->advance();
        self->phase_ = COMPOSITED_CHECK;
        return;
      }
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      const int broad = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
      const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
      PaintAnswerBuffer<> answers;
      ProbeSource source;
      const PaintApplyVerdict verdict = CollectPaintAnswers(*self->composited_, query, answers, source);
      BoundaryLocalApplyInfo info;
      info.paintKind = LOCAL_APPLY_PAINT_COMPOSITED;
      const bool gate = verdict.canSkipBroadPaint(info);
      std::fprintf(self->log_, "zstack_gate=%d broad_requests=%d\r", gate ? 1 : 0, broad);
      self->recordArm("contained-text-history", verdict.refusedCount() == 0, COMPOSITED_CHECK);
      self->recordArm("zstack", !gate && broad > 0, EDIT_WRITE);
    }
    static void OnEditIdle(Window *window, double, void *data)
    {
      PaintDamageConfig *self = static_cast<PaintDamageConfig *>(data);
      if (self->phase_ != EDIT_WRITE && self->phase_ != EDIT_CHECK)
        return;
      ToolboxWindow *native = window ? window->asToolboxWindow() : 0;
      if (!native || !window->scene() || !self->edit_)
      {
        self->finish(false);
        return;
      }
      ToolboxScenePlatformController *controller = static_cast<ToolboxScenePlatformController *>(
          loka::dsl::testing::SceneTestAccess::platformController(*window->scene()));
      FindEdit edit;
      enumerateAttachedResidents(self->edit_, edit);
      ToolboxScenePlatformController::EditTextGeometry geometry;
      if (!controller || !controller->queryEditTextGeometryForTesting(edit.context, geometry))
      {
        self->finish(false);
        return;
      }
      if (self->phase_ == EDIT_WRITE)
      {
        const Rect chrome = edit.context->chromeRect();
        const bool clipped = geometry.view.bottom < chrome.bottom - 1;
        std::fprintf(self->log_, "edit_initially_clipped=%d\r", clipped ? 1 : 0);
        if (!clipped)
        {
          self->finish(false);
          return;
        }
        self->editGeometry_ = geometry;
        native->requestInvalidateRect(chrome);
        self->phase_ = EDIT_CHECK;
        return;
      }
      const bool same = EqualRect(&geometry.destination, &self->editGeometry_.destination)
                        && EqualRect(&geometry.view, &self->editGeometry_.view);
      std::fprintf(self->log_, "edit_replay_preserves_projection=%d\r", same ? 1 : 0);
      self->recordArm("edit-replay", same, COMPLETE);
      self->finish(true);
    }
  };
}

int main(int, char **)
{
  return loka::standalone_tests::RunStandaloneFlowWithConfig<PaintDamageConfig>(0, 0);
}
