#include "ScrapbookNavigationTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>

#include "app/PlatformContext.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/scene/Scene.hpp"
#include "core/String.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"

// MainNode's package is still Toolbox-only on the portable-presentation
// branch. Compile the real node in a private test namespace against this
// deterministic probe so the null-platform pin exercises the shipped button
// composition and binding without pulling the later ByteSource slice down
// the stack. The renamed namespace also keeps this probe distinct from the
// real ScrapbookPackage linked by the next branch.
namespace scrapbook_navigation_test
{
  const std::size_t kPageCount = 5;

  struct PagePresentation
  {
    PagePresentation()
        : page(-1),
          bag(0),
          bagBlob(),
          image(),
          text(),
          caption(),
          badge(),
          isImage(false)
    {
    }

    int page;
    std::size_t bag;
    loka::core::resource::Blob bagBlob;
    loka::core::resource::Image image;
    loka::core::String text;
    loka::core::String caption;
    loka::core::String badge;
    bool isImage;
  };

  class ScrapbookPackage
  {
  public:
    ScrapbookPackage() {}

    ~ScrapbookPackage() {}

    bool open(PlatformContext *)
    {
      return true;
    }

    bool preparePage(int page, PagePresentation &out)
    {
      ++prepareCount_;
      out = PagePresentation();
      out.page = page;
      out.bag = static_cast<std::size_t>(page + 1);
      out.caption = loka::core::String::FromInt(page + 1)
                    + loka::core::String::Literal(" / 5");
      out.badge = loka::core::String::Literal("TEXT");
      return true;
    }

    void commitPage(const PagePresentation &page)
    {
      currentPage_ = page.page;
    }

    void close()
    {
      currentPage_ = -1;
    }

    loka::core::resource::Image refusedBadgeImage() const
    {
      return loka::core::resource::Image::Empty();
    }

    bool hasCurrentPage() const
    {
      return currentPage_ >= 0;
    }

    int currentPage() const
    {
      return currentPage_;
    }

    static void resetPrepareCount()
    {
      prepareCount_ = 0;
      currentPage_ = -1;
    }

    static unsigned long prepareCount()
    {
      return prepareCount_;
    }

  private:
    ScrapbookPackage(const ScrapbookPackage &);
    ScrapbookPackage &operator=(const ScrapbookPackage &);

    static unsigned long prepareCount_;
    static int currentPage_;
  };

  unsigned long ScrapbookPackage::prepareCount_ = 0;
  int ScrapbookPackage::currentPage_ = -1;
} // namespace scrapbook_navigation_test

#define LOKA_SCRAPBOOK_UI_PACKAGE_HPP
#define scrapbook scrapbook_navigation_test
#include "../example/ScrapbookUI/src/MainNode.hpp"
#undef scrapbook
#undef LOKA_SCRAPBOOK_UI_PACKAGE_HPP

namespace
{
  loka::app::ButtonNode *FindRenderedButton(loka::app::scene::Node *node,
                                            const char *label,
                                            int &matches)
  {
    if (!node)
    {
      return 0;
    }

    loka::app::ButtonNode *result = 0;
    loka::app::ButtonNode *button = node->asButtonNode();
    if (button && button->props.text_
        && button->props.text_->get().equals(loka::core::String::Literal(label)))
    {
      ++matches;
      result = button;
    }

    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
         child;
         child = child->nextInComposition)
    {
      loka::app::ButtonNode *childResult = FindRenderedButton(child, label, matches);
      if (childResult)
      {
        result = childResult;
      }
    }
    return result;
  }

  loka::app::ButtonNode *RequireRenderedButton(loka::app::scene::Scene &scene,
                                               const char *label)
  {
    int matches = 0;
    loka::app::ButtonNode *button = FindRenderedButton(
        loka::dsl::testing::SceneTestAccess::rootNode(scene), label, matches);
    LOKA_VERIFY(matches == 1);
    LOKA_VERIFY(button != 0);
    LOKA_VERIFY(button->props.onClick_ != 0);
    return button;
  }

  void ClickRenderedButton(scrapbook_navigation_test::MainNode &mainNode,
                           loka::app::ButtonNode &button)
  {
    loka::core::StateTrackerGuard guard(mainNode.tracker());
    button.props.onClick_->emit();
  }

  void VerifyPage(const scrapbook_navigation_test::MainNode &mainNode,
                  int expectedPage,
                  unsigned long expectedPrepareCount)
  {
    int publishedPage = -1;
    LOKA_VERIFY(mainNode.queryCurrentPageIndex(publishedPage));
    LOKA_VERIFY(publishedPage == expectedPage);
    LOKA_VERIFY(scrapbook_navigation_test::ScrapbookPackage::prepareCount()
                == expectedPrepareCount);
  }
} // namespace

void testScrapbookRenderedNavigationButtonsMoveAndStopAtEndpoints()
{
  scrapbook_navigation_test::ScrapbookPackage::resetPrepareCount();
  NullPlatformContext context;
  scrapbook_navigation_test::MainProps props;
  props.platformContext(&context);
  loka::app::scene::NodeDefinition<scrapbook_navigation_test::MainProps,
                                   scrapbook_navigation_test::MainNode>
      mainDefinition(props);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(mainDefinition.clone());
  scene.mount(&platform);
  scene.updateAttached(true);

  scrapbook_navigation_test::MainNode *mainNode =
      static_cast<scrapbook_navigation_test::MainNode *>(
          loka::dsl::testing::SceneTestAccess::rootNode(scene));
  LOKA_VERIFY(mainNode != 0);
  loka::app::ButtonNode *previous = RequireRenderedButton(scene, "Previous");
  loka::app::ButtonNode *next = RequireRenderedButton(scene, "Next");

  VerifyPage(*mainNode, 0, 1);
  ClickRenderedButton(*mainNode, *next);
  VerifyPage(*mainNode, 1, 2);
  ClickRenderedButton(*mainNode, *previous);
  VerifyPage(*mainNode, 0, 3);

  // The old ScrollBar emitted nothing when an endpoint gesture settled on
  // its current value. The replacement buttons must preserve that no-op.
  ClickRenderedButton(*mainNode, *previous);
  VerifyPage(*mainNode, 0, 3);

  for (int page = 1; page < static_cast<int>(scrapbook_navigation_test::kPageCount); ++page)
  {
    ClickRenderedButton(*mainNode, *next);
    VerifyPage(*mainNode, page, static_cast<unsigned long>(page + 3));
  }
  ClickRenderedButton(*mainNode, *next);
  VerifyPage(*mainNode, 4, 7);

  std::printf("testScrapbookRenderedNavigationButtonsMoveAndStopAtEndpoints passed\n");
}
