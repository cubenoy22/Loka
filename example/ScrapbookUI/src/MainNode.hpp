#ifndef LOKA_SCRAPBOOK_UI_MAIN_NODE_HPP
#define LOKA_SCRAPBOOK_UI_MAIN_NODE_HPP

#include <cassert>

#include "ScrapbookFlowAdapters.hpp"
#include "ScrapbookSceneIds.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "app/scene/state/NodeState.hpp"

namespace scrapbook
{
  using loka::app::Button;

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
          selectedPage_(0),
          package_(),
          refusedPage_(),
          refusedBadgeVisible_(),
          refusedPageNumber_(),
          refusedBadgeImage_(),
          showImage_(),
          showText_(),
          image_(),
          pageText_(),
          caption_(),
          badge_(),
          previousPage_(),
          nextPage_(),
          pageFlow_()
    {
      this->state(this->refusedPage_, -1);
      this->state(this->refusedBadgeVisible_, false);
      this->state(this->refusedPageNumber_, loka::core::String());
      this->state(this->refusedBadgeImage_, loka::core::resource::Image::Empty());
      this->state(this->showImage_, false);
      this->state(this->showText_, true);
      this->state(this->image_, loka::core::resource::Image::Empty());
      this->state(this->pageText_, loka::core::String::Literal("Loading package..."));
      this->state(this->caption_, loka::core::String::Literal("-- / 5"));
      this->state(this->badge_, loka::core::String::Literal("TEXT"));
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

    /** Programmatically selects and loads a page through the same path as the
        page navigation controls. Values outside the package range clamp to an endpoint. */
    void selectPage(int page)
    {
      if (page < 0)
      {
        page = 0;
      }
      else if (page >= static_cast<int>(kPageCount))
      {
        page = static_cast<int>(kPageCount - 1);
      }
      if (page == this->selectedPage_)
      {
        return;
      }
      this->selectedPage_ = page;
      if (this->package_.isOpen())
      {
        this->loadSelectedPage();
      }
    }

    /** Returns the selector's current zero-based page. */
    int selectedPage() const
    {
      return this->selectedPage_;
    }

    /** Returns the last refused zero-based page, or -1 when none is shown. */
    int refusedPage() const
    {
      return this->refusedPage_.get();
    }

    /** True while the persistent refusal badge section is selected into the
        caption row. */
    bool isRefusedBadgeVisible() const
    {
      return this->refusedPage_.get() >= 0;
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      if (this->package_.isOpen())
      {
        return;
      }
      (void)composition;
      this->props.assertInitialized();
      this->package_.open(this->props.platformContext_);
      this->refusedBadgeImage_.set(this->package_.refusedBadgeImage());
      this->pageFlow_.set(buildFlow(*this)).withTracker(static_cast<loka::core::PushStateTracker *>(this->tracker()));
      this->bindActionForUi(this->previousPage_, &MainNode::showPreviousPage);
      this->bindActionForUi(this->nextPage_, &MainNode::showNextPage);
      this->loadSelectedPage();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &composition)
    {
      this->pageFlow_.clear();
      this->package_.close();
      loka::app::scene::ComposableNode::detachNode(composition);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      composition.declare(
          VStack().TEST_ID(scene_ids::Root()).alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING)
          << (Box().TEST_ID(scene_ids::PageContent()).size(300, 170)
              << (Show(*this->showImage_.state())
                  << ImageView()
                         .image(this->image_.state())
                         .attr(ImageViewAttr().sizePolicy(IMAGE_VIEW_SIZE_FILL_PARENT).fit(IMAGE_FIT_CONTAIN)))
              << (Show(*this->showText_.state())
                  << Text(this->pageText_.state())
                         .TEST_ID(scene_ids::PageText())
                         .attr(TextAttr().fontSize(18).wrap(TEXT_WRAP_WORD).truncation(TEXT_TRUNCATION_NONE))))
          << (HStack().alignVertical(VERTICAL_ALIGNMENT_CENTER)
              << Text(this->caption_.state()).TEST_ID(scene_ids::PageCaption())
              << Text(this->badge_.state()).attr(TextAttr().weight(TEXT_WEIGHT_BOLD))
              << (Show(*this->refusedBadgeVisible_.state())
                  << ImageView()
                         .image(this->refusedBadgeImage_.state())
                         .size(16, 16)
                         .attr(ImageViewAttr().sizePolicy(IMAGE_VIEW_SIZE_INTRINSIC).fit(IMAGE_FIT_CONTAIN))
                  << Text(this->refusedPageNumber_.state()).attr(TextAttr().weight(TEXT_WEIGHT_BOLD))))
          << (HStack() << Button("Previous", &this->previousPage_).TEST_ID(scene_ids::PreviousButton())
                       << Button("Next", &this->nextPage_).TEST_ID(scene_ids::NextButton())));
    }

  private:
    static void OnPageLoaded(const PagePresentation &page, void *userData);
    static loka::dsl::FlowHandleResult OnPageLoadFailure(const loka::dsl::FlowError &error, void *userData);
    static PageFlowChain buildFlow(MainNode &self);

    void loadSelectedPage()
    {
      this->pageFlow_.runResult();
    }

    void showPreviousPage()
    {
      this->selectPage(this->selectedPage_ - 1);
    }

    void showNextPage()
    {
      this->selectPage(this->selectedPage_ + 1);
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
      if (this->refusedPage_.get() == page.page)
      {
        this->setRefusedPage(-1);
      }
    }

    void setRefusedPage(int page)
    {
      this->refusedPage_.set(page);
      this->refusedBadgeVisible_.set(page >= 0);
      this->refusedPageNumber_.set(page >= 0 ? loka::core::String::FromInt(page + 1) : loka::core::String());
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

    int selectedPage_;
    ScrapbookPackage package_;
    loka::app::scene::NodeState<int> refusedPage_;
    loka::app::scene::NodeState<bool> refusedBadgeVisible_;
    loka::app::scene::NodeState<loka::core::String> refusedPageNumber_;
    loka::app::scene::NodeState<loka::core::resource::Image> refusedBadgeImage_;
    loka::app::scene::NodeState<bool> showImage_;
    loka::app::scene::NodeState<bool> showText_;
    loka::app::scene::NodeState<loka::core::resource::Image> image_;
    loka::app::scene::NodeState<loka::core::String> pageText_;
    loka::app::scene::NodeState<loka::core::String> caption_;
    loka::app::scene::NodeState<loka::core::String> badge_;
    loka::core::EmitterState previousPage_;
    loka::core::EmitterState nextPage_;
    loka::app::scene::FlowSlot<PageFlowChain> pageFlow_;
  };
} // namespace scrapbook

#include "MainNodeFlow.hpp"

#endif // LOKA_SCRAPBOOK_UI_MAIN_NODE_HPP
