#include "LazyLayoutTests.hpp"
#include "app/layout/LazyLayout.hpp"
#include "app/nodes/nestable/LazyView.hpp"
#include "app/nodes/Text.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>

namespace loka
{
  namespace testing
  {
    /** Observe the existing view-owned publications, without production counters. */
    class LazyViewAccess
    {
      core::State<app::LazyViewKey> *selection_;
      core::State<core::Frame> *viewport_;
      unsigned publications_;
      static void published(void *context)
      {
        ++static_cast<LazyViewAccess *>(context)->publications_;
      }

    public:
      template <class T>
      explicit LazyViewAccess(app::LazyViewNode<T> &view)
          : selection_(view.selection_.state()),
            viewport_(view.viewport_.state()),
            publications_(0)
      {
        this->selection_->bind(&published, this, false);
        this->viewport_->bind(&published, this, false);
      }
      ~LazyViewAccess()
      {
        this->selection_->unbind(&published, this);
        this->viewport_->unbind(&published, this);
      }
      void reset(NullScenePlatformController &platform)
      {
        this->publications_ = 0;
        platform.leafLayoutVisits_ = 0;
      }
      unsigned publications() const
      {
        return this->publications_;
      }
      unsigned count() const
      {
        return this->selection_->get().window.count;
      }
      static unsigned replacements(const app::scene::Node *before, const app::scene::Node *after)
      {
        return before != after ? 1 : 0;
      }
      static unsigned long leaves(const NullScenePlatformController &platform)
      {
        return platform.leafLayoutVisits_;
      }

    private:
      LazyViewAccess(const LazyViewAccess &);
      LazyViewAccess &operator=(const LazyViewAccess &);
    };
  } // namespace testing
} // namespace loka

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  class CounterItemNode;
  struct CounterItem : NodePropsBase<CounterItem>
  {
    typedef CounterItem TypeTag;
    typedef CounterItemNode NodeType;
    bool operator<(const PropsBase &) const
    {
      return false;
    }
    bool operator!=(const CounterItem &) const
    {
      return false;
    }
  };
  class CounterItemNode : public ComponentNodeWithProps<CounterItem>
  {
  public:
    explicit CounterItemNode(const CounterItem &p)
        : ComponentNodeWithProps<CounterItem>(p)
    {
    }
    virtual void composeChildren(NodeComposition &c)
    {
      c.declare(Text("item"));
    }
  };
  void captureScroll(unsigned short n)
  {
    PushStateTracker tracker;
    ObservableList<CounterItem> list;
    MutableState<Frame> viewport(Frame(0, 0, 200, 80));
    tracker.addState(&viewport);
    LOKA_VERIFY(list.attach(&tracker, n) == ATTACH_OK);
    for (unsigned short i = 0; i < n; ++i)
      LOKA_VERIFY(list.insert(i, CounterItem()) == EDIT_OK);
    NullScenePlatformController platform;
    Scene scene(LazyColumn(list, reservation::SeatNodes<reservation::Nodes<CounterItemNode, 1,
        reservation::Nodes<TextNode, 1> > >(), 8).cells(200, 20).viewport(viewport));
    scene.mount(&platform);
    LayoutState bounds;
    bounds.width = 200;
    bounds.height = 80;
    Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
    platform.projectLayoutForTesting(root, bounds);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    scene.flushInvalidation();
    scene.flushInvalidation();
    LazyViewNode<CounterItem> *flex = static_cast<LazyViewNode<CounterItem> *>(root);
    Node *generation = flex->childrenHead();
    {
      loka::testing::LazyViewAccess counters(*flex);
      for (unsigned step = 0; step < 2; ++step)
      {
        counters.reset(platform);
        {
          StateTrackerGuard guard(&tracker);
          viewport.set(step == 0 ? Frame(0, 20, 200, 80) : Frame(0, 21, 200, 79));
        }
        scene.flushInvalidation();
        const unsigned replacements = counters.replacements(generation, flex->childrenHead());
        const unsigned publications = counters.publications();
        const unsigned long leaves = counters.leaves(platform);
        std::printf("LazyView N=%u %s: publications=%u leaves=%lu replacements=%u\n",
                    n,
                    step == 0 ? "crossing" : "same-window",
                    publications,
                    leaves,
                    replacements);
        LOKA_VERIFY(publications == (step == 0 ? 2u : 1u));
        LOKA_VERIFY(replacements == (step == 0 ? 1u : 0u));
        generation = flex->childrenHead();
        LOKA_VERIFY(counters.count() == 6);
        LOKA_VERIFY(leaves == counters.count());
        counters.reset(platform);
        scene.flushInvalidation();
        platform.drainNativeRetirements();
        LOKA_VERIFY(counters.publications() == 0);
        LOKA_VERIFY(counters.leaves(platform) == 0);
      }
    }
    // Positive control: a structure change really does replace the generation.
    LOKA_VERIFY(list.remove(list.at(n - 1).id) == EDIT_OK);
    scene.flushInvalidation();
    LOKA_VERIFY(loka::testing::LazyViewAccess::replacements(generation, flex->childrenHead()) == 1);
    loka::dsl::testing::SceneTestAccess::unmount(scene);
    scene.flushInvalidation();
    platform.drainNativeRetirements();
  }
} // namespace

void testLazyViewFlushCounters()
{
  captureScroll(50);
  captureScroll(200);
}

void testLazyLayoutBoundsAndEdges()
{
  using namespace loka::app::layout;
  const LazyLayout grid = FixedGrid(10, 20, 2, 7, STACK_AXIS_COLUMN, 0);
  LazyWindow window = grid.indicesIn(Frame(0, 19, 20, 2));
  LOKA_VERIFY(window.first == 0 && window.count == 4);
  window = grid.indicesIn(Frame(0, 20, 20, 20));
  LOKA_VERIFY(window.first == 2 && window.count == 2);
  window = grid.indicesIn(Frame(0, 60, 20, 100));
  LOKA_VERIFY(window.first == 6 && window.count == 1);
  window = grid.indicesIn(Frame(0, 80, 20, 20));
  LOKA_VERIFY(window.count == 0);
  window = grid.indicesIn(Frame(0, -10, 20, 20));
  LOKA_VERIFY(window.first == 0 && window.count == 2);
  window = grid.indicesIn(Frame(0, INT_MIN, 20, INT_MAX));
  LOKA_VERIFY(window.count == 0);
  window = grid.indicesIn(Frame(0, INT_MAX, 20, 20));
  LOKA_VERIFY(window.count == 0);
  window = grid.indicesIn(Frame(0, 0, 0, 20));
  LOKA_VERIFY(window.count == 0);
  window = grid.indicesIn(Frame(0, 0, 20, -1));
  LOKA_VERIFY(window.count == 0);
  window = grid.indicesIn(Frame(20, 0, 20, 20));
  LOKA_VERIFY(window.count == 0);
  window = grid.indicesIn(Frame(-20, 0, 20, 20));
  LOKA_VERIFY(window.count == 0);
  window = grid.indicesIn(Frame(-1, 0, 20, 20));
  LOKA_VERIFY(window.first == 0 && window.count == 2);
  const LazyLayout row = FixedGrid(20, 10, 2, 7, STACK_AXIS_ROW, 0);
  window = row.indicesIn(Frame(19, 0, 2, 20));
  LOKA_VERIFY(window.first == 0 && window.count == 4);
  window = row.indicesIn(Frame(60, 0, 100, 20));
  LOKA_VERIFY(window.first == 6 && window.count == 1);
  const LazyLayout margin = FixedGrid(10, 20, 2, 7);
  LOKA_VERIFY(margin.margin == 1);
  window = margin.indicesIn(Frame(0, 20, 20, 20));
  LOKA_VERIFY(window.first == 0 && window.count == 6);
  window = margin.indicesIn(Frame(0, 60, 20, 20));
  LOKA_VERIFY(window.first == 4 && window.count == 3);
  const LazyLayout hugeMargin = FixedGrid(1, 1, 65535, 7, STACK_AXIS_COLUMN, 65535);
  window = hugeMargin.indicesIn(Frame(0, 0, 1, 1));
  LOKA_VERIFY(window.first == 0 && window.count == 7);
}

void testLazyLayoutValuesAndWalls()
{
  using namespace loka::app::layout;
  const LazyLayout grid = FixedGrid(10, 20, 2, 7, STACK_AXIS_COLUMN, 0);
  LazyLayout copy = grid;
  LazyExtent extent = copy.extent(7);
  LOKA_VERIFY(extent.status == LAZY_EXTENT_READY && extent.frame == Frame(0, 0, 20, 80));
  LOKA_VERIFY(copy.place(6) == Frame(0, 60, 10, 20));
  extent = copy.extent(0);
  LOKA_VERIFY(extent.status == LAZY_EXTENT_READY && extent.frame == Frame(0, 0, 0, 0));
  copy.axis = STACK_AXIS_ROW;
  LOKA_VERIFY(copy.place(6) == Frame(30, 0, 10, 20));
  extent = copy.extent(7);
  LOKA_VERIFY(extent.status == LAZY_EXTENT_READY && extent.frame == Frame(0, 0, 40, 40));
  LOKA_VERIFY(grid.place(6) == Frame(0, 60, 10, 20));
  // Queries cannot mutate the snapshot's selection bound.
  copy.extent(100);
  const LazyWindow window = copy.indicesIn(Frame(0, 0, 100, 100));
  LOKA_VERIFY(window.first == 0 && window.count == 7);
  copy.wrap = 0;
  LOKA_VERIFY(copy.extent(7).status == LAZY_EXTENT_INVALID_INPUT);
  LOKA_VERIFY(copy.place(0) == Frame());
  LOKA_VERIFY(copy.indicesIn(Frame(0, 0, 100, 100)).count == 0);
  copy = grid;
  copy.cellWidth = 0;
  LOKA_VERIFY(copy.extent(7).status == LAZY_EXTENT_INVALID_INPUT);
  LOKA_VERIFY(copy.place(0) == Frame());
  copy = grid;
  copy.cellHeight = -1;
  LOKA_VERIFY(copy.extent(7).status == LAZY_EXTENT_INVALID_INPUT);
  copy = FixedGrid(32767, 32767, 1, UINT_MAX);
  LOKA_VERIFY(copy.extent(UINT_MAX).status == LAZY_EXTENT_INT_RANGE_REFUSED);
  LOKA_VERIFY(copy.place(UINT_MAX) == Frame());
  LOKA_VERIFY(copy.indicesIn(Frame(0, 0, 1, 1)).count == 0);
}
