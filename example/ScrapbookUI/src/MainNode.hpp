#ifndef LOKA_SCRAPBOOK_UI_MAIN_NODE_HPP
#define LOKA_SCRAPBOOK_UI_MAIN_NODE_HPP

#include <cassert>

#include "ScrapbookFlowAdapters.hpp"
#include "app/RectSurface.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "app/scene/state/NodeState.hpp"

namespace scrapbook
{
  struct MainTypeTag
  {
  };

  class MainNode;

  struct MainProps : public loka::app::scene::NodePropsBase<MainProps>
  {
    typedef MainTypeTag TypeTag;
    typedef MainNode NodeType;

    MainProps()
        : platformContext_(0)
    {
    }

    MainProps &platformContext(PlatformContext *context)
    {
      this->platformContext_ = context;
      return *this;
    }

    void assertInitialized() const
    {
      assert(this->platformContext_);
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      return this->platformContext_ < other.platformContext_;
    }

    PlatformContext *platformContext_;
  };

  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
  public:
    typedef MainTypeTag TypeTag;
    typedef loka::dsl::FlowChain<int, PagePresentation> PageFlowChain;

    explicit MainNode(const MainProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(props),
          initialized_(false),
          selectedPage_(0),
          package_(),
          page_(),
          showImage_(),
          showText_(),
          image_(),
          pageText_(),
          caption_(),
          badge_(),
          pageBackdrop_(),
          pageFlip_(),
          pageFlow_()
    {
      this->state(this->page_, 0);
      this->state(this->showImage_, false);
      this->state(this->showText_, true);
      this->state(this->image_, loka::core::resource::Image::Empty());
      this->state(this->pageText_, loka::core::String::Literal("Loading package..."));
      this->state(this->caption_, loka::core::String::Literal("-- / 5"));
      this->state(this->badge_, loka::core::String::Literal("TEXT"));
      this->state(this->pageBackdrop_, loka::app::RectSurfaceModel());
    }

    /** Reports presence and index together so a refused presentation cannot
        be mistaken for page zero. */
    bool queryCurrentPageIndex(int &out) const
    {
      if (!this->package_.hasCurrentPage())
      {
        return false;
      }
      out = this->package_.currentPage();
      return true;
    }

    /** Returns the caption currently published by the presentation owner. */
    loka::core::String displayedCaption() const
    {
      return this->caption_.get();
    }

    /** Returns the text currently published by the presentation owner. */
    loka::core::String displayedPageText() const
    {
      return this->pageText_.get();
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      if (this->initialized_)
      {
        return;
      }
      (void)composition;
      this->props.assertInitialized();
      this->package_.open(this->props.platformContext_);
      this->pageFlow_.set(buildFlow(*this)).withTracker(static_cast<loka::core::PushStateTracker *>(this->tracker()));
      this->bindActionForUi(this->pageFlip_, &MainNode::loadSelectedPage);
      this->initialized_ = true;
      this->loadSelectedPage();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &composition)
    {
      this->pageFlow_.clear();
      this->package_.close();
      this->initialized_ = false;
      loka::app::scene::ComposableNode::detachNode(composition);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      composition.declare(
          VStack().alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING)
          << (Show(*this->showImage_.state())
              << ImageView()
                     .image(this->image_.state())
                     .size(300, 170)
                     .attr(ImageViewAttr().sizePolicy(IMAGE_VIEW_SIZE_FILL_PARENT).fit(IMAGE_FIT_CONTAIN)))
          << (Show(*this->showText_.state())
              << (ZStack() << RectSurface(this->pageBackdrop_.state()).size(300, 170)
                           << Text(this->pageText_.state())
                                  .attr(TextAttr().fontSize(18).wrap(TEXT_WRAP_WORD).truncation(TEXT_TRUNCATION_NONE))))
          << (HStack().alignVertical(VERTICAL_ALIGNMENT_CENTER)
              << Text(this->caption_.state()) << Text(this->badge_.state()).attr(TextAttr().weight(TEXT_WEIGHT_BOLD)))
          << ScrollBar(this->page_.state())
                 .horizontal()
                 .range(0, static_cast<int>(kPageCount - 1))
                 .onChange(&this->pageFlip_));
    }

  private:
    static void OnPageLoaded(const PagePresentation &page, void *userData);
    static loka::dsl::FlowHandleResult OnPageLoadFailure(const loka::dsl::FlowError &error, void *userData);
    static PageFlowChain buildFlow(MainNode &self);

    void loadSelectedPage()
    {
      this->selectedPage_ = this->page_.get();
      this->pageFlow_.runResult();
    }

    void publishPage(const PagePresentation &page)
    {
      this->image_.set(page.image);
      this->pageText_.set(page.text);
      this->caption_.set(page.caption);
      this->badge_.set(page.badge);
      this->showImage_.set(page.isImage);
      this->showText_.set(!page.isImage);
      // The logical display now owns the new Image. Only after that handoff
      // does the package owner close and release the previous bag ledger.
      this->package_.commitPage(page);
    }

    void publishRefusal()
    {
      this->image_.set(loka::core::resource::Image::Empty());
      this->pageText_.set(loka::core::String::Literal("Package refused."));
      this->caption_.set(loka::core::String::Literal("-- / 5"));
      this->badge_.set(loka::core::String::Literal("REFUSED"));
      this->showImage_.set(false);
      this->showText_.set(true);
    }

    bool initialized_;
    int selectedPage_;
    ScrapbookPackage package_;
    loka::app::scene::NodeState<int> page_;
    loka::app::scene::NodeState<bool> showImage_;
    loka::app::scene::NodeState<bool> showText_;
    loka::app::scene::NodeState<loka::core::resource::Image> image_;
    loka::app::scene::NodeState<loka::core::String> pageText_;
    loka::app::scene::NodeState<loka::core::String> caption_;
    loka::app::scene::NodeState<loka::core::String> badge_;
    loka::app::scene::NodeState<loka::app::RectSurfaceModel> pageBackdrop_;
    loka::core::EmitterState pageFlip_;
    loka::app::scene::FlowSlot<PageFlowChain> pageFlow_;
  };
} // namespace scrapbook

#include "MainNodeFlow.hpp"

#endif // LOKA_SCRAPBOOK_UI_MAIN_NODE_HPP
