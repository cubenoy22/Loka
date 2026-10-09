#ifndef LOKA_RETIREMENT_PROBE_LAZY_HPP
#define LOKA_RETIREMENT_PROBE_LAZY_HPP
#include "RetirementProbeLog.hpp"
#include "RetirementProbeBitmap.hpp"
#include "ScenarioWindow.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/LazyView.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "core/ObservableList.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/app/NativeResourceRetirementTestAccess.hpp"
#include "testing/core/StateTrackerTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  using loka::core::resource::Image;
  typedef loka::app::testing::NativeResourceRetirementTestAccess Ledger;
  enum { kItems = 32, kWidth = 256, kCellHeight = 32, kVisible = 8, kResidents = kVisible + 1 };

  /** Config-scoped constructor decode service. Props carry only seeds; this
      explicit test bootstrap borrow supplies the platform before item attach.
      It retains no Image, and is destroyed after all App-owned item nodes. */
  class ItemDecoder
  {
  public:
    ItemDecoder(PlatformContext &context, RetirementProbeLog &log)
        : context_(context), log_(log), decoded_(0)
    {
      assert(!active_);
      active_ = this;
    }
    ~ItemDecoder() { active_ = 0; }
    unsigned decoded() const { return this->decoded_; }
    static Image decode(unsigned char fill)
    {
      assert(active_);
      Image image;
      const loka::core::resource::Blob blob = retirement_probe::Bitmap(256, 256, fill);
      if (!active_->context_.createImageFromBlob(blob, 0, blob.size(), image)
          || image.width() != 256 || image.height() != 256)
        active_->log_.error("item-decode-failed");
      else
        ++active_->decoded_;
      return image;
    }
  private:
    ItemDecoder(const ItemDecoder &);
    ItemDecoder &operator=(const ItemDecoder &);
    static ItemDecoder *active_;
    PlatformContext &context_;
    RetirementProbeLog &log_;
    unsigned decoded_;
  };
  ItemDecoder *ItemDecoder::active_ = 0;

  class ImageItem;
  struct ItemProps : loka::app::scene::NodePropsBase<ItemProps>
  {
    typedef ItemProps TypeTag;
    typedef ImageItem NodeType;
    explicit ItemProps(short value = 0, unsigned char color = 0) : id(value), fill(color) {}
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId()) return false;
      const ItemProps &other = static_cast<const ItemProps &>(rhs);
      return this->id != other.id ? this->id < other.id : this->fill < other.fill;
    }
    bool operator!=(const ItemProps &other) const { return this->id != other.id || this->fill != other.fill; }
    short id;
    unsigned char fill;
  };

  class ImageItem : public loka::app::scene::ComponentNodeWithProps<ItemProps>
  {
  public:
    explicit ImageItem(const ItemProps &props)
        : loka::app::scene::ComponentNodeWithProps<ItemProps>(props), image_()
    {
      this->declareStates(1).state(this->image_, ItemDecoder::decode(props.fill));
    }
    virtual void composeChildren(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::ImageView().image(this->image_.state())
          .attr(loka::app::ImageViewAttr().sizePolicy(loka::app::IMAGE_VIEW_SIZE_FILL_PARENT)
                                       .fit(loka::app::IMAGE_FIT_CONTAIN)));
    }
  private:
    loka::app::scene::NodeState<Image> image_;
  };

  /** App-owned list and fixed viewport; there are no Image values upstream of items. */
  class Model
  {
    loka::core::PushStateTracker tracker_;
  public:
    Model() : tracker_(), items(), viewport(loka::core::Frame(0, 0, kWidth, kVisible * kCellHeight))
    {
      if (this->items.attach(&this->tracker_, kItems) != loka::core::ATTACH_OK) return;
      for (short i = 0; i < kItems; ++i)
        if (this->items.insert(i, ItemProps(i, static_cast<unsigned char>(32 + i * 6))) != loka::core::EDIT_OK)
          return;
    }
    loka::core::ListEditResult moveFirstToEnd()
    {
      return this->items.move(this->items.at(0).id, this->items.size() - 1);
    }
    loka::core::ObservableList<ItemProps> items;
    loka::core::State<loka::core::Frame> viewport;
  private:
    Model(const Model &);
    Model &operator=(const Model &);
  };

  class MainNode;
  struct MainProps : loka::app::scene::NodePropsBase<MainProps>
  {
    typedef MainProps TypeTag;
    typedef MainNode NodeType;
    explicit MainProps(Model *value = 0) : model(value) {}
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() && this->model < static_cast<const MainProps &>(rhs).model;
    }
    Model *model;
  };
  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
  public:
    explicit MainNode(const MainProps &props) : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(props) {}
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      using namespace loka::app;
      composition.declare(Box().size(kWidth, kVisible * kCellHeight)
          << LazyColumn(this->props.model->items,
                        reservation::SeatNodes<reservation::Nodes<ImageItem, 1,
                            reservation::Nodes<ImageViewNode, 1> > >(), 12)
                .cells(kWidth, kCellHeight).wrap(1).viewport(this->props.model->viewport));
    }
  };

  class LazyProbe : public AppConfigurable
  {
  public:
    explicit LazyProbe(PlatformContext *context)
        : AppConfigurable(context), log_(L"retirement-probe-lazy.log"), decoder_(*context, this->log_),
          model_(), app_(0), main_(0), phase_(WAIT), ticks_(0), stable_(0), lastHeld_(0)
    {
      if (this->model_.items.size() != kItems) this->log_.error("model-setup-refused");
      this->log_.note("fixture=lazy-item img=256x256 pixel_bytes=262144 visible=8 margin=1 residents=9 items=32");
    }
    bool ready() const { return this->log_.valid(); }
    int exitCode() const { return this->phase_ == DONE && this->log_.valid() ? 0 : 1; }
    void setApp(App *app) { this->app_ = app; }
    virtual void compose(AppComposition &composition)
    {
      composition << loka::scenario_tests::MakeScenarioWindow<MainProps, MainNode>(
          MainProps(&this->model_), &this->main_, 280, 300, "Lazy retirement probe",
          loka::app::IdlePolicy::interval(0.05), &OnIdle, this);
    }
  private:
    enum Phase { WAIT, SWAP, SETTLE, DONE };
    static void OnIdle(Window *window, double, void *data) { static_cast<LazyProbe *>(data)->tick(window); }
    void fail(const char *reason) { this->log_.error(reason); this->app_->quit(); }
    void sample(const char *point)
    {
      this->log_.sample(*this->getPlatformContext(), this->ticks_, point, 0, 256, 256);
    }
    void tick(Window *window)
    {
      if (!this->log_.valid()) { this->app_->quit(); return; }
      switch (this->phase_)
      {
      case WAIT:
      {
        const std::size_t held = Ledger::held(*this->getPlatformContext());
        this->stable_ = this->main_ && this->decoder_.decoded() == kResidents && held == this->lastHeld_
            ? this->stable_ + 1 : 0;
        this->lastHeld_ = held;
        if (this->stable_ == 3)
        {
          this->phase_ = SWAP;
          this->ticks_ = 0;
          this->log_.begin("lazy");
        }
        else if (++this->ticks_ == 100) this->fail("mount-timeout");
        return;
      }
      case SWAP:
      {
        this->sample("pre");
        const unsigned decoded = this->decoder_.decoded();
        if (this->model_.moveFirstToEnd() != loka::core::EDIT_OK) { this->fail("edit-refused"); return; }
        // The list publication reaches the LazyView when the turn settles, so
        // run that checkpoint now; then post sees the new generation while the
        // old one still waits for the tail's reclaim, in both revisions.
        loka::core::Operation *turn = loka::core::testing::OperationTestAccess::active();
        if (!turn) { this->fail("no-active-turn"); return; }
        turn->settle();
        loka::app::scene::Scene *scene = window ? window->scene() : 0;
        loka::app::scene::Scene *out = 0;
        loka::dsl::FlowError error;
        if (loka::dsl::testing::FlushSceneInvalidation().run(scene, out, error) != loka::dsl::FLOW_STEP_SUCCEEDED)
        { this->fail("scene-flush-failed"); return; }
        this->sample("post");
        if (this->decoder_.decoded() != decoded + kResidents)
        { this->fail("generation-did-not-replace"); return; }
        if (++this->ticks_ == 10) { this->phase_ = SETTLE; this->ticks_ = 0; }
        return;
      }
      case SETTLE:
        this->sample("settle");
        if (++this->ticks_ == 5)
        {
          this->log_.summary();
          this->phase_ = DONE;
          this->app_->quit();
        }
        return;
      case DONE:
        return;
      }
    }
    RetirementProbeLog log_;
    ItemDecoder decoder_;
    Model model_;
    App *app_;
    MainNode *main_;
    Phase phase_;
    int ticks_, stable_;
    std::size_t lastHeld_;
  };
}


#endif
