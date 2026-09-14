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
#include "app/nodes/ImageView.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
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
      this->state(this->offset_, 32);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      Column surfaces = Column()
                        << RectSurface(this->first_.state()).size(180, 50).useRegionClip(true).TEST_ID("PaintDamage.First")
                        << RectSurface(this->sibling_.state()).size(180, 50).TEST_ID("PaintDamage.Sibling");
      if (HasViewport)
        composition.declare(Box().size(240, 180)
                            << (ScrollView(this->offset_)
                                << (Column() << Box().size(180, 48) << surfaces
                                    << Text(this->text_.state()).TEST_ID("PaintDamage.ScrollText")
                                    << Box().size(180, 160))));
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
    NodeState<int> offset_;
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

  class ImageSurfaceNode;
  typedef BoundaryPropsFor<ImageSurfaceNode> ImageSurfaceProps;
  /** Only this Boundary owns and changes the overlapping surface model. */
  class ImageSurfaceNode : public StdCompositionBoundaryNodeBase<ImageSurfaceProps>
  {
  public:
    typedef ImageSurfaceProps::TypeTag TypeTag;
    explicit ImageSurfaceNode(const ImageSurfaceProps &props)
        : StdCompositionBoundaryNodeBase<ImageSurfaceProps>(props)
    {
      this->setTestId("ImageDamage.Owner");
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(0, 0, 120, 24);
      this->state(this->model_, model);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(RectSurface(this->model_.state()).size(150, 70)
                          .clearBackground(true).TEST_ID("ImageDamage.Surface"));
    }
    void advance()
    {
      RectSurfaceModel model = this->model_.get();
      model.rects[0].x = 128;
      loka::core::StateTrackerGuard guard(this->tracker());
      this->model_.set(model);
    }
  private:
    NodeState<RectSurfaceModel> model_;
  };

  class ImagePlaceholderNode;
  typedef BoundaryPropsFor<ImagePlaceholderNode> ImagePlaceholderProps;
  /** The image has a separate paint-answer scope from the changing surface. */
  class ImagePlaceholderNode : public StdCompositionBoundaryNodeBase<ImagePlaceholderProps>
  {
  public:
    typedef ImagePlaceholderProps::TypeTag TypeTag;
    explicit ImagePlaceholderNode(const ImagePlaceholderProps &props)
        : StdCompositionBoundaryNodeBase<ImagePlaceholderProps>(props) {}
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(ImageView().size(150, 70));
    }
  };

  class ImageOverlapNode;
  typedef BoundaryPropsFor<ImageOverlapNode> ImageOverlapProps;
  class ImageOverlapNode : public StdCompositionBoundaryNodeBase<ImageOverlapProps>
  {
  public:
    typedef ImageOverlapProps::TypeTag TypeTag;
    explicit ImageOverlapNode(const ImageOverlapProps &props)
        : StdCompositionBoundaryNodeBase<ImageOverlapProps>(props) {}
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Box().size(180, 110)
                          << (ScrollView()
                              << (ZStack()
                                  << loka::app::scene::Boundary<ImageSurfaceNode>()
                                  << loka::app::scene::Boundary<ImagePlaceholderNode>())));
    }
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
      // Top-origin layout puts the EditText at y=72 with height 20; the
      // viewport [24,86) leaves 14 px of chrome visible and clips 6 px.
      composition.declare(Box().size(180, 62)
                          << (ScrollView()
                              << (Column() << Box().size(150, 48)
                                  << EditText(this->text_).TEST_ID("PaintDamage.Edit"))));
    }
  private:
    NodeState<loka::core::String> text_;
  };

  class SurfaceBoundsNode;
  typedef BoundaryPropsFor<SurfaceBoundsNode> SurfaceBoundsProps;
  /** A fixed model overhangs the declared surface by eight pixels on two sides. */
  class SurfaceBoundsNode : public StdCompositionBoundaryNodeBase<SurfaceBoundsProps>
  {
  public:
    typedef SurfaceBoundsProps::TypeTag TypeTag;
    explicit SurfaceBoundsNode(const SurfaceBoundsProps &props)
        : StdCompositionBoundaryNodeBase<SurfaceBoundsProps>(props)
    {
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(52, 32, 16, 16);
      this->state(this->model_, model);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Column()
                          << (Box().size(180, 40)
                              << RectSurface(this->model_.state()).clearBackground(true).size(60, 40))
                          << (Box().size(180, 54)
                              << (ScrollView()
                                  << (Column() << Box().size(150, 48)
                                      << Text("HHHH").TEST_ID("PaintDamage.ClippedText")))));
    }
  private:
    NodeState<RectSurfaceModel> model_;
  };

  enum DrawerFixture { CELL_VIEWPORT, POPUP_VIEWPORT, CELL_OVERFLOW };

  template <DrawerFixture Kind>
  class DrawerDamageNode : public StdCompositionBoundaryNodeBase<BoundaryPropsFor<DrawerDamageNode<Kind> > >
  {
  public:
    typedef BoundaryPropsFor<DrawerDamageNode<Kind> > Props;
    typedef typename Props::TypeTag TypeTag;
    explicit DrawerDamageNode(const Props &props) : StdCompositionBoundaryNodeBase<Props>(props) {}
    virtual void composeNode(NodeComposition &composition)
    {
      if (Kind == CELL_VIEWPORT)
        composition.declare(Row() << (Box().size(180, 62)
                            << (ScrollView().TEST_ID("Drawer.Viewport")
                                << (Column() << (Box().size(100, 24) << Cell("One"))
                                    << (Box().size(100, 24) << Cell("Two"))
                                    << (Box().size(100, 24) << Cell("Three"))))) << Text("Rail"));
      else if (Kind == POPUP_VIEWPORT)
        composition.declare(Box().size(96, 62)
                            << (ScrollView().TEST_ID("Drawer.Viewport") << PopupMenu()));
      else
        composition.declare(Row() << Box().size(42, 24)
                            << (Box().size(8, 24) << Cell("WWWWWWW")));
    }
  };

  /** Finite pixel snapshot owned by the fixture, never a production cache. */
  class PopupRow
  {
  public:
    PopupRow() : pixels_() {}
    void capture()
    {
      for (short x = 12; x < 156; ++x)
        this->pixels_[x - 12] = GetPixel(x, 31) != 0;
    }
    bool matches() const
    {
      for (short x = 12; x < 156; ++x)
        if (this->pixels_[x - 12] != (GetPixel(x, 31) != 0))
          return false;
      return true;
    }
  private:
    bool pixels_[144];
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
          initial_(), marker_(), scrollTextMarker_(), gate_(false), editGeometry_(), paintWindow_(0), compositedWindow_(0), editWindow_(0), plain_(0), plainWindow_(0), boundsWindow_(0), cellWindow_(0), popupWindow_(0), overflowWindow_(0), imageWindow_(0), popupRow_()
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
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 160).title("Surface bounds")
                               .scene(loka::scenario_tests::ObservedMainDefinition<SurfaceBoundsProps, SurfaceBoundsNode>(
                                   SurfaceBoundsProps(), 0))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->boundsWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 160).title("Cell replay")
                               .scene(loka::scenario_tests::ObservedMainDefinition<DrawerDamageNode<CELL_VIEWPORT>::Props, DrawerDamageNode<CELL_VIEWPORT> >(
                                   DrawerDamageNode<CELL_VIEWPORT>::Props(), 0))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->cellWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 160).title("Popup replay")
                               .scene(loka::scenario_tests::ObservedMainDefinition<DrawerDamageNode<POPUP_VIEWPORT>::Props, DrawerDamageNode<POPUP_VIEWPORT> >(
                                   DrawerDamageNode<POPUP_VIEWPORT>::Props(), 0))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->popupWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 160).title("Cell ink")
                               .scene(loka::scenario_tests::ObservedMainDefinition<DrawerDamageNode<CELL_OVERFLOW>::Props, DrawerDamageNode<CELL_OVERFLOW> >(
                                   DrawerDamageNode<CELL_OVERFLOW>::Props(), 0))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->overflowWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 160).title("Image replay")
                               .scene(loka::scenario_tests::ObservedMainDefinition<ImageOverlapProps, ImageOverlapNode>(
                                   ImageOverlapProps(), 0))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->imageWindow_);
    }

  private:
    enum Phase { SETTLE, WRITE, CHECK, PLAIN_WRITE, PLAIN_CHECK, INVALIDATED_WRITE, INVALIDATED_CHECK,
                 COMPOSITED_WRITE, COMPOSITED_CHECK, EDIT_WRITE, EDIT_CHECK, BOUNDS_SHOW, BOUNDS_CHECK, CELL_SHOW, CELL_REPLAY, CELL_CHECK,
                 POPUP_SHOW, POPUP_REPLAY, POPUP_CHECK, OVERFLOW_SHOW, OVERFLOW_REPLAY, OVERFLOW_CHECK, IMAGE_SHOW, IMAGE_WRITE, IMAGE_CHECK, COMPLETE };
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
    Point scrollTextMarker_;
    bool gate_;
    ToolboxScenePlatformController::EditTextGeometry editGeometry_;

    Window *paintWindow_;
    Window *compositedWindow_;
    Window *editWindow_;
    PlainDamageNode *plain_;
    Window *plainWindow_;
    Window *boundsWindow_;
    Window *cellWindow_;
    Window *popupWindow_;
    Window *overflowWindow_;
    Window *imageWindow_;
    PopupRow popupRow_;

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
        same finite sequence over the explicit fixture borrows. */
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
      case BOUNDS_SHOW: case BOUNDS_CHECK:
        target = self->boundsWindow_;
        break;
      case CELL_SHOW: case CELL_REPLAY: case CELL_CHECK:
        target = self->cellWindow_;
        break;
      case POPUP_SHOW: case POPUP_REPLAY: case POPUP_CHECK:
        target = self->popupWindow_;
        break;
      case OVERFLOW_SHOW: case OVERFLOW_REPLAY: case OVERFLOW_CHECK:
        target = self->overflowWindow_;
        break;
      case IMAGE_SHOW: case IMAGE_WRITE: case IMAGE_CHECK:
        target = self->imageWindow_;
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
      else if (target == self->editWindow_)
        OnEditIdle(target, elapsed, data);
      else if (target == self->boundsWindow_)
        OnBoundsIdle(target, self);
      else if (target == self->imageWindow_)
        OnImageIdle(target, self);
      else
        OnDrawerIdle(target, self);
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
          self->recordArm(HasViewport ? "viewport-exact-setup" : "plain-exact-setup", false,
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
          self->recordArm(HasViewport ? "viewport-exact-setup" : "plain-exact-setup", false,
                          HasViewport ? PLAIN_WRITE : INVALIDATED_WRITE);
          return;
        }
        if (HasViewport)
        {
          // Root y=24, leading space=48, initial offset=32: surface y=40.
          // Text follows both 50px surfaces at y=140. Find real sibling ink.
          bool foundText = false;
          for (short y = 140; y < 156 && !foundText; ++y)
            for (short x = 12; x < 180 && !foundText; ++x)
              if (GetPixel(x, y))
              {
                self->scrollTextMarker_.h = x;
                self->scrollTextMarker_.v = y;
                foundText = true;
              }
          const bool scrolled = source.firstDamage.y == 40;
          const bool oldBlack = GetPixel(20, 48) != 0;
          const bool newWhite = GetPixel(32, 48) == 0;
          self->recordArm("viewport-scroll-replay-setup",
                          scrolled && foundText && oldBlack && newWhite, WRITE);
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
        if (HasViewport)
        {
          const bool textPreserved = GetPixel(self->scrollTextMarker_.h, self->scrollTextMarker_.v) != 0;
          const bool oldErased = GetPixel(20, 48) == 0;
          const bool newPainted = GetPixel(32, 48) != 0;
          std::fprintf(self->log_, "scrolled_text_preserved=%d old_erased=%d new_painted=%d\r",
                       textPreserved ? 1 : 0, oldErased ? 1 : 0, newPainted ? 1 : 0);
          self->recordArm("viewport-scroll-replay",
                          whole == 0 && rects >= 1 && textPreserved && oldErased && newPainted
                          && stats.totalRenderCalls == self->initial_.totalRenderCalls
                          && stats.totalRenderDirtyCalls > self->initial_.totalRenderDirtyCalls, CHECK);
        }
        SetPort(previousPort);
        std::fprintf(self->log_, "delivery=EXACT\r");
        self->recordArm(HasViewport ? "viewport-exact" : "plain-exact",
                        self->gate_ && siblingPreserved && whole == 0
                        && (HasViewport ? rects >= 1 : rects == 1),
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
      const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
      // The ZStack is not classified composited by the Boundary, so the write
      // is delivered exactly; renderDirty's ZStack branch re-renders the dirty
      // rect under a clip so the overlapping Text keeps its ink. Sample the
      // Text ink right of the moved sprite (sprite now spans x 28-40 at y 52-64).
      GrafPtr zPort;
      GetPort(&zPort);
      SetPort(native->window());
      bool textInk = false;
      for (short y = 50; y < 62; ++y)
        for (short x = 44; x < 58; ++x)
          textInk = textInk || GetPixel(x, y) != 0;
      SetPort(zPort);
      std::fprintf(self->log_, "zstack_gate=%d broad_requests=%d rect_requests=%d text_ink=%d\r", gate ? 1 : 0, broad, rects, textInk ? 1 : 0);
      self->recordArm("contained-text-history", verdict.refusedCount() == 0, COMPOSITED_CHECK);
      self->recordArm("zstack-overlap-replay", !gate && broad == 0 && rects >= 1 && textInk, EDIT_WRITE);
    }
    static void OnBoundsIdle(Window *window, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == BOUNDS_SHOW)
      {
        // Keep this window hidden until all earlier fixture arms have finished.
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = BOUNDS_CHECK;
        return;
      }
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      // The root layout starts at (12, 24). Both samples lie in the sprite's
      // row; only the first lies inside the 60 x 40 surface.
      const bool insideBlack = GetPixel(12 + 56, 24 + 36) != 0;
      const bool outsideWhite = GetPixel(12 + 64, 24 + 36) == 0;
      // The second seat starts at y=64: its viewport ends at 118, while
      // Text starts at 112 with baseline 124. H has ink on both sampled rows.
      bool textInsideBlack = false;
      bool textOutsideWhite = true;
      for (short x = 12; x < 60; ++x)
      {
        textInsideBlack = GetPixel(x, 117) != 0 || textInsideBlack;
        textOutsideWhite = GetPixel(x, 119) == 0 && textOutsideWhite;
      }
      SetPort(previousPort);
      TextNode *text = 0;
      loka::dsl::FlowError error;
      loka::dsl::testing::LookupNodeById<TextNode>(
          window->scene(), "PaintDamage.ClippedText", text, error);
      const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
      const PaintAnswer answer = text && text->getContext()
          ? static_cast<NativeNodeContext *>(text->getContext())->queryPaintDamage(query)
          : PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
      const bool clippedHistory = answer.kind == PAINT_ANSWER_EXACT && answer.damage.y == 112;
      std::fprintf(self->log_, "text_inside_black=%d outside_white=%d clipped_history=%d\r",
                   textInsideBlack ? 1 : 0, textOutsideWhite ? 1 : 0, clippedHistory ? 1 : 0);
      self->recordArm("text-clipped-to-viewport",
                      textInsideBlack && textOutsideWhite && clippedHistory, BOUNDS_CHECK);
      std::fprintf(self->log_, "surface_inside_black=%d outside_white=%d\r",
                   insideBlack ? 1 : 0, outsideWhite ? 1 : 0);
      self->recordArm("surface-bounds-clip", insideBlack && outsideWhite, CELL_SHOW);
    }
    static void OnDrawerIdle(Window *window, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == CELL_SHOW || self->phase_ == POPUP_SHOW || self->phase_ == OVERFLOW_SHOW)
      {
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = self->phase_ == CELL_SHOW ? CELL_REPLAY
                       : self->phase_ == POPUP_SHOW ? POPUP_REPLAY : OVERFLOW_REPLAY;
        return;
      }
      ToolboxScenePlatformController *controller = window->scene()
          ? static_cast<ToolboxScenePlatformController *>(
              loka::dsl::testing::SceneTestAccess::platformController(*window->scene())) : 0;
      if (!controller)
      {
        self->finish(false);
        return;
      }
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      if (self->phase_ == CELL_REPLAY || self->phase_ == POPUP_REPLAY)
      {
        const bool cell = self->phase_ == CELL_REPLAY;
        Node *viewportNode = 0;
        loka::dsl::FlowError error;
        loka::dsl::testing::LookupNodeById<Node>(window->scene(), "Drawer.Viewport", viewportNode, error);
        ScrollViewNode *viewport = viewportNode ? viewportNode->asScrollViewNode() : 0;
        if (!viewport)
        {
          SetPort(previousPort);
          self->finish(false);
          return;
        }
        // Isolate replay while c-1 retains the production viewport fallback.
        // Retire only the native scrollbar after full presentation; the
        // drawer's captured projection and clipped hit rows remain intact.
        controller->destroyViewportScrollBarControl(viewport, viewport->nativeLifetimeHint());
        Rect damage;
        SetRect(&damage, 12, cell ? 72 : 24, cell ? 112 : 156, cell ? 96 : 44);
        if (!cell)
          self->popupRow_.capture();
        bool ink = cell ? GetPixel(12, 80) != 0 && GetPixel(12, 90) == 0
                        : GetPixel(12, 31) != 0;
        if (!cell)
        {
          bool labelInk = false;
          for (short x = 16; x < 55; ++x)
            labelInk = labelInk || GetPixel(x, 31) != 0;
          ink = ink && labelInk;
        }
        self->initial_ = controller->debugStatsForTesting();
        native->requestInvalidateRect(damage);
        SetPort(previousPort);
        self->recordArm(cell ? "cell-replay-setup" : "popup-replay-setup", ink,
                        cell ? CELL_CHECK : POPUP_CHECK);
        return;
      }
      if (self->phase_ == CELL_CHECK || self->phase_ == POPUP_CHECK)
      {
        const bool cell = self->phase_ == CELL_CHECK;
        const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
        const bool replayed = stats.totalRenderCalls == self->initial_.totalRenderCalls
                              && stats.totalRenderDirtyCalls > self->initial_.totalRenderDirtyCalls
                              && stats.windowFlushDirtyCount > self->initial_.windowFlushDirtyCount;
        bool pass = cell ? GetPixel(12, 80) != 0 && GetPixel(12, 90) == 0
                         : self->popupRow_.matches();
        // The projection clip ends at x=108; the 16px scrollbar only
        // reduces the child layout width, not the viewport clip.
        // The retired scrollbar no longer contributes native ink here.
        if (!cell)
          for (short y = 24; y < 42; ++y)
            for (short x = 108; x < 156; ++x)
              pass = pass && GetPixel(x, y) == 0;
        SetPort(previousPort);
        std::fprintf(self->log_, "drawer_dirty_replayed=%d pixels_match=%d\r", replayed ? 1 : 0, pass ? 1 : 0);
        self->recordArm(cell ? "cell-replay-clipped" : "popup-replay-geometry", replayed && pass,
                        cell ? POPUP_SHOW : OVERFLOW_SHOW);
        return;
      }
      if (self->phase_ == OVERFLOW_REPLAY)
      {
        // A Cell-only registry must reach the Cell replay loop.
        Rect damage;
        SetRect(&damage, 60, 24, 68, 48);
        self->initial_ = controller->debugStatsForTesting();
        native->requestInvalidateRect(damage);
        self->phase_ = OVERFLOW_CHECK;
        SetPort(previousPort);
        return;
      }
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      self->recordArm("cell-only-dirty-replay",
                      stats.totalRenderCalls == self->initial_.totalRenderCalls
                      && stats.totalRenderDirtyCalls > self->initial_.totalRenderDirtyCalls, OVERFLOW_CHECK);
      // The narrow Cell is [60,68) x [24,48). Its centered seven-W label
      // is wider than the box in the same font used by the production drawer.
      const unsigned char label[] = {7, 'W', 'W', 'W', 'W', 'W', 'W', 'W'};
      bool outsideWhite = true;
      for (short y = 25; y < 47; ++y)
        for (short x = 12; x < 120; ++x)
          if (x < 60 || x >= 68)
            outsideWhite = outsideWhite && GetPixel(x, y) == 0;
      const bool setup = StringWidth(label) > 8 && GetPixel(60, 24) != 0;
      SetPort(previousPort);
      self->recordArm("cell-ink-clipped-to-rect", setup && outsideWhite, IMAGE_SHOW);
    }
    static void OnImageIdle(Window *window, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == IMAGE_SHOW)
      {
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = IMAGE_WRITE;
        return;
      }
      ToolboxScenePlatformController *controller = window->scene()
          ? static_cast<ToolboxScenePlatformController *>(
              loka::dsl::testing::SceneTestAccess::platformController(*window->scene())) : 0;
      if (!controller)
      {
        self->finish(false);
        return;
      }
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      if (self->phase_ == IMAGE_WRITE)
      {
        Node *ownerNode = 0;
        Node *surfaceNode = 0;
        loka::dsl::FlowError error;
        loka::dsl::testing::LookupNodeById<Node>(
            window->scene(), "ImageDamage.Owner", ownerNode, error);
        loka::dsl::testing::LookupNodeById<Node>(
            window->scene(), "ImageDamage.Surface", surfaceNode, error);
        ImageSurfaceNode *owner = ownerNode && ownerNode->asBoundary()
            ? static_cast<ImageSurfaceNode *>(ownerNode) : 0;
        RectSurfaceNode *surface = surfaceNode ? surfaceNode->asRectSurfaceNode() : 0;
        const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
        const PaintAnswer answer = surface && surface->getContext()
            ? static_cast<NativeNodeContext *>(surface->getContext())->queryPaintDamage(query)
            : PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
        bool foundInk = false;
        if (owner && answer.kind == PAINT_ANSWER_EXACT)
        {
          // Sample the placeholder interior, excluding its frame. The old
          // sprite covers this area; its new position starts at local x=128.
          for (short y = answer.damage.y + 2; y < answer.damage.y + 14 && !foundInk; ++y)
            for (short x = answer.damage.x + 6; x < answer.damage.x + 100 && !foundInk; ++x)
              if (GetPixel(x, y))
              {
                self->marker_.h = x;
                self->marker_.v = y;
                foundInk = true;
              }
        }
        std::fprintf(self->log_, "image_placeholder_setup=%d\r", foundInk ? 1 : 0);
        self->recordArm("image-overlap-replay-setup", foundInk, IMAGE_CHECK);
        if (foundInk)
        {
          self->initial_ = controller->debugStatsForTesting();
          owner->advance();
        }
        SetPort(previousPort);
        if (!foundInk)
          self->finish(false);
        return;
      }
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
      const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
      const bool ink = GetPixel(self->marker_.h, self->marker_.v) != 0;
      const bool replayed = stats.totalRenderCalls == self->initial_.totalRenderCalls
                            && stats.totalRenderDirtyCalls > self->initial_.totalRenderDirtyCalls
                            && stats.windowFlushDirtyCount > self->initial_.windowFlushDirtyCount;
      SetPort(previousPort);
      std::fprintf(self->log_, "image_whole_window=%d rect_requests=%d placeholder_ink=%d dirty_replayed=%d\r",
                   whole, rects, ink ? 1 : 0, replayed ? 1 : 0);
      self->recordArm("image-overlap-replay", whole == 0 && rects >= 1 && ink && replayed, COMPLETE);
      self->finish(true);
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
        // A missing native geometry is a fixture failure, not a silent end.
        self->recordArm("edit-geometry", false, COMPLETE);
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
      self->recordArm("edit-replay", same, BOUNDS_SHOW);
    }
  };
}

int main(int, char **)
{
  return loka::standalone_tests::RunStandaloneFlowWithConfig<PaintDamageConfig>(0, 0);
}
