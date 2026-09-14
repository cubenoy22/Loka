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
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
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

  class OffscreenDamageNode;
  typedef BoundaryPropsFor<OffscreenDamageNode> OffscreenDamageProps;
  /** One column overflows without truncation or wrapping, like HelloWorld. */
  class OffscreenDamageNode : public StdCompositionBoundaryNodeBase<OffscreenDamageProps>
  {
  public:
    typedef OffscreenDamageProps::TypeTag TypeTag;
    explicit OffscreenDamageNode(const OffscreenDamageProps &props)
        : StdCompositionBoundaryNodeBase<OffscreenDamageProps>(props)
    {
      this->state(this->sibling_, loka::core::String::Literal("MMMM"));
      this->state(this->text_, loka::core::String::Literal("MMMM"));
      this->state(this->hidden_, loka::core::String::Literal("MMMM"));
      this->state(this->edit_, loka::core::String::Literal("Edit"));
      this->state(this->offset_, 0);
      this->state(this->button_, loka::core::String::Literal("Button"));
      this->items_.push_back(loka::core::String::Literal("Fruit"));
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Box().size(180, 110)
                          << (ScrollView(this->offset_)
                              << (Column() << Text(this->sibling_.state())
                                  << Text(this->text_.state()).TEST_ID("Offscreen.VisibleText")
                                  << Box().size(150, 140)
                                  << Text(this->hidden_.state()).TEST_ID("Offscreen.Text")
                                  << EditText(this->edit_).TEST_ID("Offscreen.Edit")
                                  << PopupMenu(this->items_).TEST_ID("Offscreen.Popup")
                                  << loka::app::Button(this->button_.state()).TEST_ID("Offscreen.Button"))));
    }
    void writeSibling()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->sibling_.set(loka::core::String::Literal("IIII"));
    }
    void writeText(bool hidden, const char *value = "IIII")
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      if (hidden)
        this->hidden_.set(loka::core::String::Literal(value));
      else
        this->text_.set(loka::core::String::Literal("IIII"));
    }
    void writeButton(bool wider)
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->button_.set(loka::core::String::Literal(wider ? "Much wider button title" : "Button"));
    }
    void reveal()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->offset_.set(160);
    }
  private:
    NodeState<loka::core::String> sibling_, text_, hidden_, edit_, button_;
    NodeState<int> offset_;
    loka::Vector<loka::core::String> items_;
  };

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

  class EditZStackNode;
  typedef BoundaryPropsFor<EditZStackNode> EditZStackProps;
  /** The live edit and its later overlapping surface share one Boundary. */
  class EditZStackNode : public StdCompositionBoundaryNodeBase<EditZStackProps>
  {
  public:
    typedef EditZStackProps::TypeTag TypeTag;
    explicit EditZStackNode(const EditZStackProps &props)
        : StdCompositionBoundaryNodeBase<EditZStackProps>(props)
    {
      this->state(this->text_, loka::core::String::Literal(""));
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(4, 4, 12, 12);
      this->state(this->model_, model);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(ZStack()
                          << EditText(this->text_).TEST_ID("PaintDamage.Edit")
                          << RectSurface(this->model_.state()).size(16, 20).TEST_ID("EditZStack.Surface"));
    }
    void advance()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->text_.set(loka::core::String::Literal("    HHHH"));
    }
  private:
    NodeState<loka::core::String> text_;
    NodeState<RectSurfaceModel> model_;
  };

  class EditExactNode;
  typedef BoundaryPropsFor<EditExactNode> EditExactProps;
  /** One owner makes the paint walk visit the edit and both siblings. */
  class EditExactNode : public StdCompositionBoundaryNodeBase<EditExactProps>
  {
  public:
    typedef EditExactProps::TypeTag TypeTag;
    explicit EditExactNode(const EditExactProps &props)
        : StdCompositionBoundaryNodeBase<EditExactProps>(props)
    {
      this->state(this->text_, loka::core::String::Literal(""));
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(4, 4, 12, 12);
      this->state(this->model_, model);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Box().size(180, 140)
                          << (ScrollView()
                              << (Column()
                                  << EditText(this->text_).TEST_ID("PaintDamage.Edit")
                                  << Text("Sibling ink").TEST_ID("EditExact.Text")
                                  << RectSurface(this->model_.state()).size(150, 40)
                                      .clearBackground(true).TEST_ID("EditExact.Surface"))));
    }
    void advance()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->text_.set(loka::core::String::Literal("HHHH"));
    }
  private:
    NodeState<loka::core::String> text_;
    NodeState<RectSurfaceModel> model_;
  };

  class ButtonExactNode;
  typedef BoundaryPropsFor<ButtonExactNode> ButtonExactProps;
  /** Each arm owns its button input and unchanged siblings in one Boundary. */
  class ButtonExactNode : public StdCompositionBoundaryNodeBase<ButtonExactProps>
  {
  public:
    typedef ButtonExactProps::TypeTag TypeTag;
    explicit ButtonExactNode(const ButtonExactProps &props)
        : StdCompositionBoundaryNodeBase<ButtonExactProps>(props)
    {
      this->state(this->enabled_, true);
      this->state(this->title_, loka::core::String::Literal("MMMMMMMM"));
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(4, 4, 12, 12);
      this->state(this->model_, model);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Column()
                          << loka::app::Button(this->title_.state()).enabled(this->enabled_.state()).controlTag(911).TEST_ID("ButtonExact.Button")
                          << Text("Sibling ink").TEST_ID("ButtonExact.Text")
                          << RectSurface(this->model_.state()).size(150, 40)
                              .clearBackground(true).TEST_ID("ButtonExact.Surface"));
    }
    void advance(bool label)
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      if (label)
        this->title_.set(loka::core::String::Literal("IIIIIIII"));
      else
        this->enabled_.set(false);
    }
  private:
    NodeState<bool> enabled_;
    NodeState<loka::core::String> title_;
    NodeState<RectSurfaceModel> model_;
  };

  /** Samples only the title interior, excluding the standard CDEF border. */
  class ButtonTitlePixels
  {
  public:
    ButtonTitlePixels() : pixels_() {}
    void capture(const Rect &rect)
    {
      for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 48; ++x)
          this->pixels_[y * 48 + x] = GetPixel(rect.left + 8 + x, rect.top + 3 + y) != 0;
    }
    int erasedInk(const Rect &rect) const
    {
      int count = 0;
      for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 48; ++x)
          if (this->pixels_[y * 48 + x] && !GetPixel(rect.left + 8 + x, rect.top + 3 + y))
            ++count;
      return count;
    }
  private:
    bool pixels_[48 * 8];
  };

  class PopupExactNode;
  typedef BoundaryPropsFor<PopupExactNode> PopupExactProps;
  /** One Boundary owns the popup inputs and the siblings visited on each write. */
  class PopupExactNode : public StdCompositionBoundaryNodeBase<PopupExactProps>
  {
  public:
    typedef PopupExactProps::TypeTag TypeTag;
    explicit PopupExactNode(const PopupExactProps &props)
        : StdCompositionBoundaryNodeBase<PopupExactProps>(props)
    {
      this->items_.push_back(loka::core::String::Literal(""));
      this->items_.push_back(loka::core::String::Literal("HHHH"));
      this->state(this->selection_, 0);
      this->state(this->text_, loka::core::String::Literal("Sibling ink"));
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(4, 4, 12, 12);
      this->state(this->model_, model);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Box().size(180, 140)
                          << (ScrollView()
                              << (Column()
                                  << PopupMenu(this->items_).selectedIndex(this->selection_)
                                      .TEST_ID("PopupExact.Popup")
                                  << Text(this->text_.state())
                                  << RectSurface(this->model_.state()).size(150, 40)
                                      .clearBackground(true).TEST_ID("PopupExact.Surface"))));
    }
    void writeSibling()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->text_.set(loka::core::String::Literal("Changed ink"));
    }
    void selectNext()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->selection_.set(1);
    }
  private:
    loka::Vector<loka::core::String> items_;
    NodeState<int> selection_;
    NodeState<loka::core::String> text_;
    NodeState<RectSurfaceModel> model_;
  };

  /** Both distant drawers share the Boundary whose answers are collected. */
  template <bool Popup>
  class HistoryNode : public StdCompositionBoundaryNodeBase<BoundaryPropsFor<HistoryNode<Popup> > >
  {
  public:
    typedef BoundaryPropsFor<HistoryNode<Popup> > Props;
    typedef typename Props::TypeTag TypeTag;
    explicit HistoryNode(const Props &props) : StdCompositionBoundaryNodeBase<Props>(props)
    {
      this->state(this->first_, loka::core::String::Literal("AAAA"));
      this->state(this->second_, loka::core::String::Literal("    "));
      this->state(this->selection_, 0);
      this->items_.push_back(loka::core::String::Literal("    "));
      this->items_.push_back(loka::core::String::Literal("HHHH"));
      RectSurfaceModel model;
      model.rectCount = 1;
      model.rects[0] = RectSprite(4, 4, 12, 12);
      this->state(this->model_, model);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      Column drawers = Column() << Text(this->first_.state()).TEST_ID("History.A")
                               << Box().size(150, 48);
      if (Popup)
        drawers << PopupMenu(this->items_).selectedIndex(this->selection_).TEST_ID("History.B");
      else
        drawers << Text(this->second_.state()).TEST_ID("History.B");
      drawers << Box().size(150, 24)
              << RectSurface(this->model_.state()).size(150, 24)
                  .clearBackground(true).TEST_ID("History.Surface");
      composition.declare(ZStack() << drawers);
    }
    void writeFirst()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->first_.set(loka::core::String::Literal("HHHH"));
    }
    void writeSecond()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      if (Popup)
        this->selection_.set(1);
      else
        this->second_.set(loka::core::String::Literal("HHHH"));
    }
  private:
    NodeState<loka::core::String> first_;
    NodeState<loka::core::String> second_;
    NodeState<int> selection_;
    NodeState<RectSurfaceModel> model_;
    loka::Vector<loka::core::String> items_;
  };

  /** Snapshot two finite face rows in the fixture's window coordinates. */
  class PopupFaceRows
  {
  public:
    PopupFaceRows() : frame_(), label_() {}
    void capture(const Rect &rect)
    {
      for (int x = 0; x < 128; ++x)
      {
        this->frame_[x] = GetPixel(rect.left + x, rect.top + 7) != 0;
        this->label_[x] = GetPixel(rect.left + x, rect.top + 9) != 0;
      }
    }
    bool matches(const Rect &rect, bool label) const
    {
      for (int x = 0; x < 128; ++x)
        if ((label ? this->label_[x] : this->frame_[x])
            != (GetPixel(rect.left + x, rect.top + (label ? 9 : 7)) != 0))
          return false;
      return true;
    }
  private:
    bool frame_[128];
    bool label_[128];
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
          initial_(), marker_(), scrollTextMarker_(), gate_(false), editGeometry_(), paintWindow_(0), compositedWindow_(0), editWindow_(0), plain_(0), plainWindow_(0), boundsWindow_(0), cellWindow_(0), popupWindow_(0), overflowWindow_(0), imageWindow_(0), popupRow_(), editExact_(0), editExactWindow_(0), editZStack_(0), editZStackWindow_(0), popupExact_(0), popupExactWindow_(0), popupFaceRows_(), history_(0), historyWindow_(0), popupHistory_(0), popupHistoryWindow_(0), buttonEnabled_(0), buttonEnabledWindow_(0), buttonLabel_(0), buttonLabelWindow_(0), buttonTitlePixels_(), offscreen_(0), offscreenWindow_(0)
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
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 180).title("Edit exact")
                               .scene(loka::scenario_tests::ObservedMainDefinition<EditExactProps, EditExactNode>(
                                   EditExactProps(), &this->editExact_))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->editExactWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 180).title("Edit ZStack order")
                               .scene(loka::scenario_tests::ObservedMainDefinition<EditZStackProps, EditZStackNode>(
                                   EditZStackProps(), &this->editZStack_))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->editZStackWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 180).title("Popup exact")
                               .scene(loka::scenario_tests::ObservedMainDefinition<PopupExactProps, PopupExactNode>(
                                   PopupExactProps(), &this->popupExact_))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->popupExactWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 220).title("History replay")
                               .scene(loka::scenario_tests::ObservedMainDefinition<HistoryNode<false>::Props, HistoryNode<false> >(
                                   HistoryNode<false>::Props(), &this->history_))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->historyWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 220).title("History replay")
                               .scene(loka::scenario_tests::ObservedMainDefinition<HistoryNode<true>::Props, HistoryNode<true> >(
                                   HistoryNode<true>::Props(), &this->popupHistory_))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->popupHistoryWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 180).title("Button enabled exact")
                               .scene(loka::scenario_tests::ObservedMainDefinition<ButtonExactProps, ButtonExactNode>(
                                   ButtonExactProps(), &this->buttonEnabled_))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->buttonEnabledWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 180).title("Button label exact")
                               .scene(loka::scenario_tests::ObservedMainDefinition<ButtonExactProps, ButtonExactNode>(
                                   ButtonExactProps(), &this->buttonLabel_))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->buttonLabelWindow_);
      composition << ObservedWindowDefinition(WindowProps().frame(350, 250, 220, 180).title("Column overflow exact")
                               .scene(loka::scenario_tests::ObservedMainDefinition<OffscreenDamageProps, OffscreenDamageNode>(
                                   OffscreenDamageProps(), &this->offscreen_))
                               .visible(false).idlePolicy(IdlePolicy::everyTick())
                               .onIdle(&PaintDamageConfig::DispatchIdle, this), &this->offscreenWindow_);
    }

  private:
    enum Phase { SETTLE, WRITE, CHECK, PLAIN_WRITE, PLAIN_CHECK, INVALIDATED_WRITE, INVALIDATED_CHECK,
                 COMPOSITED_WRITE, COMPOSITED_CHECK, EDIT_WRITE, EDIT_CHECK, BOUNDS_SHOW, BOUNDS_CHECK, CELL_SHOW, CELL_REPLAY, CELL_CHECK,
                 POPUP_SHOW, POPUP_REPLAY, POPUP_CHECK, OVERFLOW_SHOW, OVERFLOW_REPLAY, OVERFLOW_CHECK, IMAGE_SHOW, IMAGE_WRITE, IMAGE_CHECK, EDIT_EXACT_SHOW, EDIT_EXACT_WRITE, EDIT_EXACT_CHECK, EDIT_ZSTACK_SHOW, EDIT_ZSTACK_WRITE, EDIT_ZSTACK_CHECK, POPUP_EXACT_SHOW, POPUP_SIBLING_WRITE, POPUP_SIBLING_CHECK, POPUP_SELECTION_CHECK, HISTORY_SHOW, HISTORY_FIRST, HISTORY_SECOND, HISTORY_CHECK,
                 POPUP_HISTORY_SHOW, POPUP_HISTORY_FIRST, POPUP_HISTORY_SECOND, POPUP_HISTORY_CHECK, BUTTON_ENABLED_SHOW, BUTTON_ENABLED_WRITE, BUTTON_ENABLED_CHECK,
                 BUTTON_LABEL_SHOW, BUTTON_LABEL_WRITE, BUTTON_LABEL_CHECK,
                 OFFSCREEN_SHOW, OFFSCREEN_WRITE, OFFSCREEN_SIBLING_CHECK, OFFSCREEN_TEXT_CHECK, OFFSCREEN_HIDDEN_CHECK, OFFSCREEN_REVEAL_CHECK, OFFSCREEN_REVEALED_WRITE_CHECK, COMPLETE };
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
    EditExactNode *editExact_;
    Window *editExactWindow_;
    EditZStackNode *editZStack_;
    Window *editZStackWindow_;
    PopupExactNode *popupExact_;
    Window *popupExactWindow_;
    PopupFaceRows popupFaceRows_;
    HistoryNode<false> *history_;
    Window *historyWindow_;
    HistoryNode<true> *popupHistory_;
    Window *popupHistoryWindow_;
    ButtonExactNode *buttonEnabled_;
    Window *buttonEnabledWindow_;
    ButtonExactNode *buttonLabel_;
    Window *buttonLabelWindow_;
    ButtonTitlePixels buttonTitlePixels_;
    OffscreenDamageNode *offscreen_;
    Window *offscreenWindow_;

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
      case EDIT_EXACT_SHOW: case EDIT_EXACT_WRITE: case EDIT_EXACT_CHECK:
        target = self->editExactWindow_;
        break;
      case EDIT_ZSTACK_SHOW: case EDIT_ZSTACK_WRITE: case EDIT_ZSTACK_CHECK:
        target = self->editZStackWindow_;
        break;
      case POPUP_EXACT_SHOW: case POPUP_SIBLING_WRITE: case POPUP_SIBLING_CHECK: case POPUP_SELECTION_CHECK:
        target = self->popupExactWindow_;
        break;
      case HISTORY_SHOW: case HISTORY_FIRST: case HISTORY_SECOND: case HISTORY_CHECK:
        target = self->historyWindow_;
        break;
      case POPUP_HISTORY_SHOW: case POPUP_HISTORY_FIRST: case POPUP_HISTORY_SECOND: case POPUP_HISTORY_CHECK:
        target = self->popupHistoryWindow_;
        break;
      case BUTTON_ENABLED_SHOW: case BUTTON_ENABLED_WRITE: case BUTTON_ENABLED_CHECK:
        target = self->buttonEnabledWindow_;
        break;
      case BUTTON_LABEL_SHOW: case BUTTON_LABEL_WRITE: case BUTTON_LABEL_CHECK:
        target = self->buttonLabelWindow_;
        break;
      case OFFSCREEN_SHOW: case OFFSCREEN_WRITE: case OFFSCREEN_SIBLING_CHECK:
      case OFFSCREEN_TEXT_CHECK: case OFFSCREEN_HIDDEN_CHECK: case OFFSCREEN_REVEAL_CHECK: case OFFSCREEN_REVEALED_WRITE_CHECK:
        target = self->offscreenWindow_;
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
      else if (target == self->editExactWindow_)
        OnEditExactIdle(target, self);
      else if (target == self->editZStackWindow_)
        OnEditZStackIdle(target, self);
      else if (target == self->popupExactWindow_)
        OnPopupExactIdle(target, self);
      else if (target == self->historyWindow_)
        OnHistoryIdle(target, self->history_, self);
      else if (target == self->popupHistoryWindow_)
        OnHistoryIdle(target, self->popupHistory_, self);
      else if (target == self->buttonEnabledWindow_)
        OnButtonIdle(target, self->buttonEnabled_, false, self);
      else if (target == self->buttonLabelWindow_)
        OnButtonIdle(target, self->buttonLabel_, true, self);
      else if (target == self->offscreenWindow_)
        OnOffscreenIdle(target, self);
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
        std::fprintf(self->log_, "delivery=%s reason=%d kind=%d\r",
                     whole == 0 ? "EXACT" : "FULL",
                     static_cast<int>(stats.lastPaintRefusalReason),
                     static_cast<int>(stats.lastPaintRefusalKind));
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
          const int top = answer.damage.y + 2;
          const int bottom = answer.damage.y + 14;
          const int left = answer.damage.x + 6;
          const int right = answer.damage.x + 100;
          for (int y = top; y < bottom && !foundInk; ++y)
            for (int x = left; x < right && !foundInk; ++x)
              if (GetPixel(static_cast<short>(x), static_cast<short>(y)))
              {
                self->marker_.h = static_cast<short>(x);
                self->marker_.v = static_cast<short>(y);
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
      self->recordArm("image-overlap-replay", whole == 0 && rects >= 1 && ink && replayed, EDIT_EXACT_SHOW);
    }
    static void OnEditExactIdle(Window *window, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == EDIT_EXACT_SHOW)
      {
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = EDIT_EXACT_WRITE;
        return;
      }
      ToolboxScenePlatformController *controller = window->scene()
          ? static_cast<ToolboxScenePlatformController *>(
              loka::dsl::testing::SceneTestAccess::platformController(*window->scene())) : 0;
      FindEdit edit;
      enumerateAttachedResidents(self->editExact_, edit);
      ToolboxScenePlatformController::EditTextGeometry geometry;
      if (!controller || !self->editExact_
          || !controller->queryEditTextGeometryForTesting(edit.context, geometry))
      {
        self->recordArm("edittext-exact-geometry", false, COMPLETE);
        self->finish(false);
        return;
      }
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      if (self->phase_ == EDIT_EXACT_WRITE)
      {
        Node *surface = 0;
        TextNode *text = 0;
        loka::dsl::FlowError error;
        loka::dsl::testing::LookupNodeById<Node>(window->scene(), "EditExact.Surface", surface, error);
        loka::dsl::testing::LookupNodeById<TextNode>(window->scene(), "EditExact.Text", text, error);
        const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
        const PaintAnswer surfaceAnswer = surface && surface->getContext()
            ? static_cast<NativeNodeContext *>(surface->getContext())->queryPaintDamage(query)
            : PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
        const PaintAnswer textAnswer = text && text->getContext()
            ? static_cast<NativeNodeContext *>(text->getContext())->queryPaintDamage(query)
            : PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
        bool foundText = false;
        if (textAnswer.kind == PAINT_ANSWER_EXACT)
          for (int y = textAnswer.damage.y; y < textAnswer.damage.y + 14 && !foundText; ++y)
            for (int x = textAnswer.damage.x; x < textAnswer.damage.x + 60 && !foundText; ++x)
              if (GetPixel(static_cast<short>(x), static_cast<short>(y)))
              {
                self->scrollTextMarker_.h = static_cast<short>(x);
                self->scrollTextMarker_.v = static_cast<short>(y);
                foundText = true;
              }
        self->marker_.h = static_cast<short>(surfaceAnswer.damage.x + 8);
        self->marker_.v = static_cast<short>(surfaceAnswer.damage.y + 8);
        bool editWhite = true;
        for (int y = geometry.view.top; y < geometry.view.bottom; ++y)
          for (int x = geometry.view.left; x < geometry.view.right; ++x)
            editWhite = GetPixel(static_cast<short>(x), static_cast<short>(y)) == 0 && editWhite;
        const bool setup = surfaceAnswer.kind == PAINT_ANSWER_EXACT && foundText && editWhite
                           && GetPixel(self->marker_.h, self->marker_.v) != 0;
        SetPort(previousPort);
        self->recordArm("edittext-exact-setup", setup, EDIT_EXACT_CHECK);
        if (!setup)
        {
          self->finish(false);
          return;
        }
        self->editGeometry_ = geometry;
        self->initial_ = controller->debugStatsForTesting();
        self->editExact_->advance();
        return;
      }
      bool editInk = false;
      for (int y = geometry.view.top; y < geometry.view.bottom; ++y)
        for (int x = geometry.view.left; x < geometry.view.right; ++x)
          editInk = GetPixel(static_cast<short>(x), static_cast<short>(y)) != 0 || editInk;
      const bool sprite = GetPixel(self->marker_.h, self->marker_.v) != 0;
      const bool textInk = GetPixel(self->scrollTextMarker_.h, self->scrollTextMarker_.v) != 0;
      SetPort(previousPort);
      std::string value;
      const bool newValue = controller->queryEditTextValueForTesting(edit.context, value) && value == "HHHH";
      const bool sameGeometry = EqualRect(&geometry.view, &self->editGeometry_.view)
                                && EqualRect(&geometry.destination, &self->editGeometry_.destination);
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
      const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
      std::fprintf(self->log_, "edittext_whole_window=%d rect_requests=%d sprite_preserved=%d text_preserved=%d new_text_visible=%d\r",
                   whole, rects, sprite ? 1 : 0, textInk ? 1 : 0, editInk && newValue ? 1 : 0);
      self->recordArm("edittext-exact", whole == 0 && rects >= 1 && sprite && textInk
                      && editInk && newValue && sameGeometry, EDIT_EXACT_CHECK);
      const PaintQuery settled = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
      const PaintAnswer unchanged = edit.context->queryPaintDamage(settled);
      self->recordArm("edittext-presented-empty", unchanged.kind == PAINT_ANSWER_EXACT
                      && unchanged.damage.width == 0 && unchanged.damage.height == 0, EDIT_EXACT_CHECK);
      PaintQuery pending = settled;
      pending.placement = PLACEMENT_PENDING;
      PaintQuery foreign = settled;
      ++foreign.scope.ownerKey;
      self->recordArm("edittext-placement-refuses",
                      edit.context->queryPaintDamage(pending).kind == PAINT_ANSWER_REFUSED
                      && edit.context->queryPaintDamage(foreign).kind == PAINT_ANSWER_REFUSED, EDIT_EXACT_CHECK);
      // Revoke the installed binding while the context still exists. History
      // alone must not keep granting EXACT after the native TE has retired.
      controller->retireNodeContext(edit.context, edit.context->lifetimeHint());
      self->recordArm("edittext-native-retired-refuses",
                      !controller->queryEditTextGeometryForTesting(edit.context, geometry)
                      && edit.context->queryPaintDamage(settled).kind == PAINT_ANSWER_REFUSED, EDIT_ZSTACK_SHOW);
    }
    static void OnEditZStackIdle(Window *window, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == EDIT_ZSTACK_SHOW)
      {
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = EDIT_ZSTACK_WRITE;
        return;
      }
      ToolboxScenePlatformController *controller = window->scene()
          ? static_cast<ToolboxScenePlatformController *>(
              loka::dsl::testing::SceneTestAccess::platformController(*window->scene())) : 0;
      FindEdit edit;
      enumerateAttachedResidents(self->editZStack_, edit);
      ToolboxScenePlatformController::EditTextGeometry geometry;
      if (!controller || !self->editZStack_
          || !controller->queryEditTextGeometryForTesting(edit.context, geometry))
      {
        self->recordArm("edittext-zstack-order-geometry", false, COMPLETE);
        self->finish(false);
        return;
      }
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      if (self->phase_ == EDIT_ZSTACK_WRITE)
      {
        Node *surface = 0;
        loka::dsl::FlowError error;
        loka::dsl::testing::LookupNodeById<Node>(window->scene(), "EditZStack.Surface", surface, error);
        const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
        const PaintAnswer answer = surface && surface->getContext()
            ? static_cast<NativeNodeContext *>(surface->getContext())->queryPaintDamage(query)
            : PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
        // The later sprite occupies local [4,16) x [4,16). The narrow
        // surface leaves the right TE strip clear for the new H glyphs.
        self->marker_.h = static_cast<short>(answer.damage.x + 10);
        self->marker_.v = static_cast<short>(answer.damage.y + 10);
        // Leading spaces keep the sample free of glyph ink on a bad replay.
        const unsigned char prefix[] = {4, ' ', ' ', ' ', ' '};
        const bool blankPrefix = StringWidth(prefix) > self->marker_.h - geometry.view.left;
        const bool overlaps = geometry.view.left < answer.damage.x + 4
                              && geometry.view.right >= answer.damage.x + 16
                              && geometry.view.top <= answer.damage.y + 4
                              && geometry.view.bottom >= answer.damage.y + 16;
        bool uncoveredWhite = true;
        for (int y = geometry.view.top; y < geometry.view.bottom; ++y)
          for (int x = answer.damage.x + 16; x < geometry.view.right; ++x)
            uncoveredWhite = GetPixel(static_cast<short>(x), static_cast<short>(y)) == 0 && uncoveredWhite;
        const bool sprite = GetPixel(self->marker_.h, self->marker_.v) != 0;
        SetPort(previousPort);
        std::fprintf(self->log_, "edittext_zstack_setup_sprite_black=%d uncovered_white=%d overlaps_te=%d te=(%d,%d,%d,%d) sample=(%d,%d)\r",
                     sprite ? 1 : 0, uncoveredWhite ? 1 : 0, overlaps ? 1 : 0,
                     geometry.view.left, geometry.view.top, geometry.view.right, geometry.view.bottom,
                     self->marker_.h, self->marker_.v);
        const bool setup = answer.kind == PAINT_ANSWER_EXACT && overlaps && blankPrefix && uncoveredWhite && sprite;
        self->recordArm("edittext-zstack-order-setup", setup, EDIT_ZSTACK_CHECK);
        if (!setup)
        {
          self->finish(false);
          return;
        }
        self->editGeometry_ = geometry;
        self->initial_ = controller->debugStatsForTesting();
        self->editZStack_->advance();
        return;
      }
      bool newInk = false;
      // Reuse the setup's all-white strip, excluding the sprite and chrome.
      for (int y = geometry.view.top; y < geometry.view.bottom; ++y)
        for (int x = self->marker_.h + 6; x < geometry.view.right; ++x)
          newInk = GetPixel(static_cast<short>(x), static_cast<short>(y)) != 0 || newInk;
      const bool sprite = GetPixel(self->marker_.h, self->marker_.v) != 0;
      SetPort(previousPort);
      std::string value;
      const bool newValue = controller->queryEditTextValueForTesting(edit.context, value) && value == "    HHHH";
      const bool sameGeometry = EqualRect(&geometry.view, &self->editGeometry_.view)
                                && EqualRect(&geometry.destination, &self->editGeometry_.destination);
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
      const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
      const bool dirtyFlushed = stats.totalRenderDirtyCalls > self->initial_.totalRenderDirtyCalls
                                && stats.windowFlushDirtyCount > self->initial_.windowFlushDirtyCount;
      std::fprintf(self->log_, "edittext_zstack_whole_window=%d rect_requests=%d sprite_black=%d new_text_visible=%d dirty_flushed=%d same_geometry=%d\r",
                   whole, rects, sprite ? 1 : 0, newInk && newValue ? 1 : 0,
                   dirtyFlushed ? 1 : 0, sameGeometry ? 1 : 0);
      self->recordArm("edittext-zstack-order", whole == 0 && rects >= 1 && sprite
                      && newInk && newValue && sameGeometry && dirtyFlushed, POPUP_EXACT_SHOW);
    }
    static void OnPopupExactIdle(Window *window, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == POPUP_EXACT_SHOW)
      {
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = POPUP_SIBLING_WRITE;
        return;
      }
      ToolboxScenePlatformController *controller = window->scene()
          ? static_cast<ToolboxScenePlatformController *>(
              loka::dsl::testing::SceneTestAccess::platformController(*window->scene())) : 0;
      Node *popup = 0;
      Node *surface = 0;
      loka::dsl::FlowError error;
      loka::dsl::testing::LookupNodeById<Node>(window->scene(), "PopupExact.Popup", popup, error);
      loka::dsl::testing::LookupNodeById<Node>(window->scene(), "PopupExact.Surface", surface, error);
      ToolboxPopupMenuContext *context = popup && popup->getContext()
          ? static_cast<ToolboxPopupMenuContext *>(popup->getContext()) : 0;
      if (!controller || !context || !surface || !surface->getContext() || !self->popupExact_)
      {
        self->recordArm("popup-exact-geometry", false, COMPLETE);
        self->finish(false);
        return;
      }
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      const Rect rect = context->rect();
      if (self->phase_ == POPUP_SIBLING_WRITE)
      {
        const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
        const PaintAnswer answer = static_cast<NativeNodeContext *>(surface->getContext())->queryPaintDamage(query);
        self->marker_.h = static_cast<short>(answer.damage.x + 8);
        self->marker_.v = static_cast<short>(answer.damage.y + 8);
        const bool setup = answer.kind == PAINT_ANSWER_EXACT && rect.right - rect.left == 128
                           && GetPixel(rect.left, rect.top + 7) != 0
                           && GetPixel(self->marker_.h, self->marker_.v) != 0;
        self->popupFaceRows_.capture(rect);
        SetPort(previousPort);
        self->recordArm("popup-exact-setup", setup, POPUP_SIBLING_CHECK);
        if (!setup)
        {
          self->finish(false);
          return;
        }
        self->initial_ = controller->debugStatsForTesting();
        self->popupExact_->writeSibling();
        return;
      }
      const bool sibling = self->phase_ == POPUP_SIBLING_CHECK;
      const bool frameSame = self->popupFaceRows_.matches(rect, false);
      const bool labelSame = self->popupFaceRows_.matches(rect, true);
      const bool sprite = GetPixel(self->marker_.h, self->marker_.v) != 0;
      SetPort(previousPort);
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
      const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
      std::fprintf(self->log_, "popup_%s_whole_window=%d rect_requests=%d frame_row_same=%d label_row_same=%d sprite_preserved=%d\r",
                   sibling ? "sibling" : "selection", whole, rects, frameSame ? 1 : 0, labelSame ? 1 : 0, sprite ? 1 : 0);
      self->recordArm(sibling ? "popup-sibling-exact" : "popup-selection-exact",
                      whole == 0 && rects >= 1 && sprite && (sibling ? frameSame && labelSame : !labelSame),
                      sibling ? POPUP_SELECTION_CHECK : COMPLETE);
      if (sibling)
      {
        self->initial_ = controller->debugStatsForTesting();
        self->popupExact_->selectNext();
      }
      else
      {
        const PaintQuery settled = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
        const PaintAnswer unchanged = context->queryPaintDamage(settled);
        self->recordArm("popup-presented-empty", unchanged.kind == PAINT_ANSWER_EXACT
                        && unchanged.damage.width == 0 && unchanged.damage.height == 0, COMPLETE);
        PaintQuery pending = settled;
        pending.placement = PLACEMENT_PENDING;
        PaintQuery foreign = settled;
        ++foreign.scope.ownerKey;
        self->recordArm("popup-placement-refuses",
                        context->queryPaintDamage(pending).kind == PAINT_ANSWER_REFUSED
                        && context->queryPaintDamage(foreign).kind == PAINT_ANSWER_REFUSED, COMPLETE);
        PopupMenuNode *popupNode = popup->asPopupMenuNode();
        loka::core::MutableState<int> *selection = popupNode->props.selectedIndex_;
        popupNode->props.selectedIndex_ = 0;
        self->recordArm("popup-unreconciled-refuses",
                        context->queryPaintDamage(settled).kind == PAINT_ANSWER_REFUSED, COMPLETE);
        popupNode->props.selectedIndex_ = selection;
        SetPort(native->window());
        {
          Rect partial = rect;
          partial.right = partial.left + 2;
          ToolboxPaintClip clip(partial);
          context->repaint();
        }
        self->recordArm("popup-partial-history-refuses",
                        context->queryPaintDamage(settled).kind == PAINT_ANSWER_REFUSED, COMPLETE);
        context->repaint();
        self->recordArm("popup-full-history-restored",
                        context->queryPaintDamage(settled).kind == PAINT_ANSWER_EXACT, COMPLETE);
        context->updateRect(rect, 16);
        self->recordArm("popup-layout-history-refuses",
                        context->queryPaintDamage(settled).kind == PAINT_ANSWER_REFUSED, COMPLETE);
        context->repaint();
        context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
        self->recordArm("popup-detached-refuses",
                        context->queryPaintDamage(settled).kind == PAINT_ANSWER_REFUSED, COMPLETE);
        SetPort(previousPort);
        self->phase_ = HISTORY_SHOW;
      }
    }
    template <bool Popup>
    static void OnHistoryIdle(Window *window, HistoryNode<Popup> *node, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == (Popup ? POPUP_HISTORY_SHOW : HISTORY_SHOW))
      {
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = Popup ? POPUP_HISTORY_FIRST : HISTORY_FIRST;
        return;
      }
      ToolboxScenePlatformController *controller = window->scene()
          ? static_cast<ToolboxScenePlatformController *>(
              loka::dsl::testing::SceneTestAccess::platformController(*window->scene())) : 0;
      Node *second = 0;
      Node *surface = 0;
      loka::dsl::FlowError error;
      loka::dsl::testing::LookupNodeById<Node>(window->scene(), "History.B", second, error);
      loka::dsl::testing::LookupNodeById<Node>(window->scene(), "History.Surface", surface, error);
      if (!controller || !node || !second || !second->getContext() || !surface || !surface->getContext())
      {
        self->finish(false);
        return;
      }
      const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
      if (self->phase_ == (Popup ? POPUP_HISTORY_FIRST : HISTORY_FIRST))
      {
        const PaintAnswer b = static_cast<NativeNodeContext *>(second->getContext())->queryPaintDamage(query);
        const PaintAnswer sprite = static_cast<NativeNodeContext *>(surface->getContext())->queryPaintDamage(query);
        if (b.kind != PAINT_ANSWER_EXACT || sprite.kind != PAINT_ANSWER_EXACT)
        {
          self->finish(false);
          return;
        }
        SetRect(&self->editGeometry_.view, b.damage.x, b.damage.y,
                b.damage.x + 36, b.damage.y + 14);
        self->marker_.h = static_cast<short>(sprite.damage.x + 8);
        self->marker_.v = static_cast<short>(sprite.damage.y + 8);
        GrafPtr previousPort;
        GetPort(&previousPort);
        SetPort(native->window());
        bool blank = true;
        for (int y = b.damage.y + 3; y < b.damage.y + 11; ++y)
          for (int x = b.damage.x + 4; x < b.damage.x + 36; ++x)
            blank = GetPixel(static_cast<short>(x), static_cast<short>(y)) == 0 && blank;
        const bool spriteVisible = GetPixel(self->marker_.h, self->marker_.v) != 0;
        SetPort(previousPort);
        self->recordArm(Popup ? "popup-history-setup" : "history-setup", blank && spriteVisible,
                        Popup ? POPUP_HISTORY_SECOND : HISTORY_SECOND);
        self->initial_ = controller->debugStatsForTesting();
        node->writeFirst();
        self->phase_ = Popup ? POPUP_HISTORY_SECOND : HISTORY_SECOND;
        return;
      }
      if (self->phase_ == (Popup ? POPUP_HISTORY_SECOND : HISTORY_SECOND))
      {
        const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
        const bool flushed = stats.totalRenderDirtyCalls > self->initial_.totalRenderDirtyCalls
                             && stats.windowFullRequestCount == self->initial_.windowFullRequestCount;
        self->recordArm(Popup ? "popup-history-first-clipped" : "history-first-clipped", flushed,
                        Popup ? POPUP_HISTORY_CHECK : HISTORY_CHECK);
        self->initial_ = stats;
        node->writeSecond();
        return;
      }
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      const Rect &rect = self->editGeometry_.view;
      bool ink = false;
      for (int y = rect.top + 3; y < rect.bottom - 3; ++y)
        for (int x = rect.left + 4; x < rect.left + 36; ++x)
          ink = GetPixel(static_cast<short>(x), static_cast<short>(y)) != 0 || ink;
      const bool sprite = GetPixel(self->marker_.h, self->marker_.v) != 0;
      SetPort(previousPort);
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
      const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
      const char *name = Popup ? "popup-history-survives" : "history-survives-clipped-render";
      std::fprintf(self->log_, "%s whole_window=%d rect_requests=%d sprite_preserved=%d new_text_visible=%d\r",
                   name, whole, rects, sprite ? 1 : 0, ink ? 1 : 0);
      self->recordArm(name, whole == 0 && rects >= 1 && sprite && ink,
                      Popup ? BUTTON_ENABLED_SHOW : POPUP_HISTORY_SHOW);
    }
    static PaintAnswer offscreenAnswer(Window *window, const char *id, const PaintQuery &query)
    {
      Node *node = 0;
      loka::dsl::FlowError error;
      loka::dsl::testing::LookupNodeById<Node>(window->scene(), id, node, error);
      return node && node->getContext()
          ? static_cast<NativeNodeContext *>(node->getContext())->queryPaintDamage(query)
          : PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
    }

    static void OnOffscreenIdle(Window *window, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == OFFSCREEN_SHOW)
      {
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = OFFSCREEN_WRITE;
        return;
      }
      ToolboxScenePlatformController *controller = window->scene()
          ? static_cast<ToolboxScenePlatformController *>(
              loka::dsl::testing::SceneTestAccess::platformController(*window->scene())) : 0;
      if (!controller || !self->offscreen_)
      {
        self->finish(false);
        return;
      }
      const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
      if (self->phase_ == OFFSCREEN_WRITE)
      {
        const char *ids[] = {"Offscreen.Text", "Offscreen.Edit", "Offscreen.Popup", "Offscreen.Button"};
        for (int i = 0; i < 4; ++i)
        {
          const PaintAnswer answer = offscreenAnswer(window, ids[i], query);
          std::fprintf(self->log_, "%s answer=%d reason=%d\r", ids[i],
                       static_cast<int>(answer.kind), static_cast<int>(answer.reason));
          self->recordArm(ids[i], answer.kind == PAINT_ANSWER_EXACT
                          && answer.damage.width == 0 && answer.damage.height == 0, OFFSCREEN_WRITE);
          PaintQuery pending = query;
          pending.placement = PLACEMENT_PENDING;
          PaintQuery foreign = query;
          ++foreign.scope.ownerKey;
          self->recordArm("offscreen-placement-refuses",
                          offscreenAnswer(window, ids[i], pending).kind == PAINT_ANSWER_REFUSED
                          && offscreenAnswer(window, ids[i], foreign).kind == PAINT_ANSWER_REFUSED, OFFSCREEN_WRITE);
        }
        self->offscreen_->writeButton(true);
        const PaintAnswer wider = offscreenAnswer(window, "Offscreen.Button", query);
        self->recordArm("offscreen-button-width-refuses", wider.kind == PAINT_ANSWER_REFUSED
                        && wider.reason == PAINT_REFUSED_PLACEMENT_UNSETTLED, OFFSCREEN_WRITE);
        self->offscreen_->writeButton(false);
        self->initial_ = controller->debugStatsForTesting();
        self->offscreen_->writeSibling();
        self->phase_ = OFFSCREEN_SIBLING_CHECK;
        return;
      }
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
      const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
      std::fprintf(self->log_, "column_phase=%d delivery=%s reason=%d kind=%d whole_window=%d rects=%d\r",
                   static_cast<int>(self->phase_), whole == 0 ? "EXACT" : "FULL",
                   static_cast<int>(stats.lastPaintRefusalReason), static_cast<int>(stats.lastPaintRefusalKind),
                   whole, rects);
      if (self->phase_ == OFFSCREEN_SIBLING_CHECK)
      {
        self->recordArm("column-offscreen-sibling-exact", whole == 0 && rects >= 1, OFFSCREEN_TEXT_CHECK);
        self->initial_ = stats;
        self->offscreen_->writeText(false);
        const PaintAnswer changed = offscreenAnswer(window, "Offscreen.VisibleText", query);
        self->recordArm("column-visible-text-damage", changed.kind == PAINT_ANSWER_EXACT
                        && changed.damage.width > 0 && changed.damage.height > 0, OFFSCREEN_TEXT_CHECK);
      }
      else if (self->phase_ == OFFSCREEN_TEXT_CHECK)
      {
        const PaintAnswer presented = offscreenAnswer(window, "Offscreen.VisibleText", query);
        self->recordArm("column-visible-text-exact", whole == 0 && rects >= 1
                        && presented.kind == PAINT_ANSWER_EXACT && presented.damage.width == 0, OFFSCREEN_HIDDEN_CHECK);
        self->initial_ = stats;
        self->offscreen_->writeText(true);
      }
      else if (self->phase_ == OFFSCREEN_HIDDEN_CHECK)
      {
        self->recordArm("column-offscreen-write-empty", whole == 0 && rects == 0, OFFSCREEN_REVEAL_CHECK);
        self->initial_ = stats;
        self->offscreen_->reveal();
      }
      else if (self->phase_ == OFFSCREEN_REVEAL_CHECK)
      {
        // Re-entry must establish history before another paint-only write.
        self->initial_ = stats;
        self->offscreen_->writeText(true, "MMMM");
        const PaintAnswer changed = offscreenAnswer(window, "Offscreen.Text", query);
        self->recordArm("column-scroll-reveals-current-text", whole > 0
                        && changed.kind == PAINT_ANSWER_EXACT && changed.damage.width > 0
                        && changed.damage.height > 0 && changed.damage.y >= 24
                        && changed.damage.y + changed.damage.height <= 134, OFFSCREEN_REVEALED_WRITE_CHECK);
      }
      else
      {
        const PaintAnswer presented = offscreenAnswer(window, "Offscreen.Text", query);
        self->recordArm("column-revealed-text-exact", whole == 0 && rects >= 1
                        && presented.kind == PAINT_ANSWER_EXACT && presented.damage.width == 0, COMPLETE);
        self->finish(true);
      }
    }

    static void OnButtonIdle(Window *window, ButtonExactNode *node, bool label, PaintDamageConfig *self)
    {
      ToolboxWindow *native = window->asToolboxWindow();
      if (self->phase_ == (label ? BUTTON_LABEL_SHOW : BUTTON_ENABLED_SHOW))
      {
        ShowWindow(native->window());
        SelectWindow(native->window());
        native->requestInvalidate();
        self->phase_ = label ? BUTTON_LABEL_WRITE : BUTTON_ENABLED_WRITE;
        return;
      }
      ToolboxScenePlatformController *controller = window->scene()
          ? static_cast<ToolboxScenePlatformController *>(
              loka::dsl::testing::SceneTestAccess::platformController(*window->scene())) : 0;
      Node *button = 0;
      Node *surface = 0;
      loka::dsl::FlowError error;
      loka::dsl::testing::LookupNodeById<Node>(window->scene(), "ButtonExact.Button", button, error);
      loka::dsl::testing::LookupNodeById<Node>(window->scene(), "ButtonExact.Surface", surface, error);
      if (!controller || !node || !button || !button->getContext() || !surface || !surface->getContext())
      {
        self->finish(false);
        return;
      }
      const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
      NativeNodeContext *context = static_cast<NativeNodeContext *>(button->getContext());
      GrafPtr previousPort;
      GetPort(&previousPort);
      SetPort(native->window());
      if (self->phase_ == (label ? BUTTON_LABEL_WRITE : BUTTON_ENABLED_WRITE))
      {
        const PaintAnswer face = context->queryPaintDamage(query);
        const PaintAnswer sprite = static_cast<NativeNodeContext *>(surface->getContext())->queryPaintDamage(query);
        SetRect(&self->editGeometry_.view, face.damage.x, face.damage.y, face.damage.x + 64, face.damage.y + 14);
        self->marker_.h = static_cast<short>(sprite.damage.x + 8);
        self->marker_.v = static_cast<short>(sprite.damage.y + 8);
        self->buttonTitlePixels_.capture(self->editGeometry_.view);
        const bool setup = face.kind == PAINT_ANSWER_EXACT && sprite.kind == PAINT_ANSWER_EXACT
                           && GetPixel(self->marker_.h, self->marker_.v);
        SetPort(previousPort);
        self->recordArm(label ? "button-label-setup" : "button-enabled-setup", setup,
                        label ? BUTTON_LABEL_CHECK : BUTTON_ENABLED_CHECK);
        self->initial_ = controller->debugStatsForTesting();
        node->advance(label);
        return;
      }
      // Disabled CDEF text removes alternating black title pixels. Changing
      // M to I removes title ink too; neither sample includes the button frame.
      const int erased = self->buttonTitlePixels_.erasedInk(self->editGeometry_.view);
      const bool sprite = GetPixel(self->marker_.h, self->marker_.v) != 0;
      SetPort(previousPort);
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      const int whole = stats.windowFullRequestCount - self->initial_.windowFullRequestCount;
      const int rects = stats.windowRectRequestCount - self->initial_.windowRectRequestCount;
      const char *name = label ? "button-label-exact" : "button-enabled-exact";
      std::fprintf(self->log_, "%s whole_window=%d rect_requests=%d title_ink_removed=%d sprite_preserved=%d\r",
                   name, whole, rects, erased, sprite ? 1 : 0);
      self->recordArm(name, whole == 0 && rects >= 1 && erased > 0 && sprite,
                      label ? COMPLETE : BUTTON_LABEL_SHOW);
      const PaintAnswer unchanged = context->queryPaintDamage(query);
      self->recordArm(label ? "button-label-presented-empty" : "button-enabled-presented-empty",
                      unchanged.kind == PAINT_ANSWER_EXACT && unchanged.damage.width == 0,
                      label ? COMPLETE : BUTTON_LABEL_SHOW);
      if (label)
      {
        PaintQuery unsettled = query;
        unsettled.placement = PLACEMENT_PENDING;
        self->recordArm("button-placement-refuses",
                        context->queryPaintDamage(unsettled).kind == PAINT_ANSWER_REFUSED, COMPLETE);
        GetPort(&previousPort);
        SetPort(native->window());
        Rect partial = self->editGeometry_.view;
        partial.right = static_cast<short>(partial.left + 24);
        {
          ToolboxPaintClip clip(partial);
          controller->drawControlsInRect(partial);
        }
        self->recordArm("button-partial-history-refuses",
                        context->queryPaintDamage(query).kind == PAINT_ANSWER_REFUSED, COMPLETE);
        controller->drawControlsInRect(native->window()->portRect);
        self->recordArm("button-full-history-restored",
                        context->queryPaintDamage(query).kind == PAINT_ANSWER_EXACT, COMPLETE);
        SetPort(previousPort);
        controller->destroyButtonControl(911, NATIVE_HINT_DEFAULT);
        self->recordArm("button-native-retired-refuses",
                        context->queryPaintDamage(query).kind == PAINT_ANSWER_REFUSED, COMPLETE);
        self->phase_ = OFFSCREEN_SHOW;
      }
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
