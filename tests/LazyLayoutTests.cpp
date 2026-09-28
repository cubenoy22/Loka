#include "LazyLayoutTests.hpp"
#include "app/layout/LazyLayout.hpp"
#include "app/nodes/nestable/LazyFlex.hpp"
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
    /** Test-only ownership of evaluator wrappers; restore before generation retirement. */
    /** Generic test door: swap a DerivedState's evaluator without a feature-named friend. */
    class DerivedStateTestAccess
    {
    public:
      template <class T>
      static typename core::DerivedState<T>::EvalFn *&evalFn(core::DerivedState<T> &state)
      {
        return state.evalFn;
      }
    };

    class LazyFlexAccess
    {
      typedef core::DerivedState<bool> Derived;
      class Evaluation : public Derived::EvalFn
      {
      public:
        Evaluation(Derived::EvalFn *source, unsigned &count)
            : source_(source),
              count_(count)
        {
        }
        virtual bool operator()()
        {
          ++this->count_;
          return (*this->source_)();
        }
        Derived::EvalFn *source_;
        unsigned &count_;
      };
      struct Watch
      {
        Derived *state;
        Evaluation *evaluation;
      };
      Watch watches_[LOKA_LAZYFLEX_MAX_ITEMS];
      unsigned count_;
      unsigned evaluations_;
      unsigned publications_;
      static void published(void *context)
      {
        ++static_cast<LazyFlexAccess *>(context)->publications_;
      }
      LazyFlexAccess(const LazyFlexAccess &);
      LazyFlexAccess &operator=(const LazyFlexAccess &);

    public:
      template <class T>
      explicit LazyFlexAccess(app::LazyGenerationNode<T> &generation)
          : count_(generation.count_),
            evaluations_(0),
            publications_(0)
      {
        for (unsigned i = 0; i < this->count_; ++i)
        {
          Watch &watch = this->watches_[i];
          watch.state = static_cast<Derived *>(generation.visible_[i]);
          watch.evaluation = new Evaluation(DerivedStateTestAccess::evalFn(*watch.state), this->evaluations_);
          DerivedStateTestAccess::evalFn(*watch.state) = watch.evaluation;
          watch.state->bind(&published, this, false);
        }
      }
      ~LazyFlexAccess()
      {
        for (unsigned i = 0; i < this->count_; ++i)
        {
          Watch &watch = this->watches_[i];
          watch.state->unbind(&published, this);
          DerivedStateTestAccess::evalFn(*watch.state) = watch.evaluation->source_;
          delete watch.evaluation;
        }
      }
      void reset(NullScenePlatformController &platform)
      {
        this->evaluations_ = this->publications_ = 0;
        platform.leafLayoutVisits_ = 0;
      }
      unsigned evaluations() const
      {
        return this->evaluations_;
      }
      unsigned publications() const
      {
        return this->publications_;
      }
      static unsigned replacements(const app::scene::Node *before, const app::scene::Node *after)
      {
        return before != after ? 1 : 0;
      }
      static unsigned long leaves(const NullScenePlatformController &platform)
      {
        return platform.leafLayoutVisits_;
      }
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
    Scene scene(LazyColumn(list).cells(200, 20).viewport(viewport));
    scene.mount(&platform);
    LayoutState bounds;
    bounds.width = 200;
    bounds.height = 80;
    Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
    platform.projectLayoutForTesting(root, bounds);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    scene.flushInvalidation();
    scene.flushInvalidation();
    LazyFlexNode<CounterItem> *flex = static_cast<LazyFlexNode<CounterItem> *>(root);
    Node *generation = flex->childrenHead();
    {
      loka::testing::LazyFlexAccess counters(*static_cast<LazyGenerationNode<CounterItem> *>(generation));
      for (unsigned step = 0; step < 2; ++step)
      {
        counters.reset(platform);
        {
          StateTrackerGuard guard(&tracker);
          viewport.set(step == 0 ? Frame(0, 20, 200, 80) : Frame(0, 21, 200, 79));
        }
        scene.flushInvalidation();
        const unsigned replacements = counters.replacements(generation, flex->childrenHead());
        const unsigned evaluations = counters.evaluations();
        const unsigned publications = counters.publications();
        const unsigned long leaves = counters.leaves(platform);
        std::printf("LazyFlex N=%u %s: evaluations=%u publications=%u leaves=%lu replacements=%u\n",
                    n,
                    step == 0 ? "crossing" : "same-window",
                    evaluations,
                    publications,
                    leaves,
                    replacements);
        LOKA_VERIFY(evaluations == n);
        LOKA_VERIFY(publications == (step == 0 ? 2u : 0u));
        LOKA_VERIFY(replacements == 0);
        LOKA_VERIFY(leaves == 4);
        counters.reset(platform);
        scene.flushInvalidation();
        platform.drainNativeRetirements();
        LOKA_VERIFY(counters.evaluations() == 0 && counters.publications() == 0);
        LOKA_VERIFY(counters.leaves(platform) == 0);
      }
    }
    // Positive control: a structure change really does replace the generation.
    LOKA_VERIFY(list.remove(list.at(n - 1).id) == EDIT_OK);
    scene.flushInvalidation();
    LOKA_VERIFY(loka::testing::LazyFlexAccess::replacements(generation, flex->childrenHead()) == 1);
    loka::dsl::testing::SceneTestAccess::unmount(scene);
    scene.flushInvalidation();
    platform.drainNativeRetirements();
  }
} // namespace

void testLazyFlexFlushCounters()
{
  captureScroll(50);
  captureScroll(200);
}

void testLazyLayoutBoundsAndEdges()
{
  using namespace loka::app::layout;
  const LazyLayout grid = FixedGrid(10, 20, 2, 7);
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
  const LazyLayout row = FixedGrid(20, 10, 2, 7, STACK_AXIS_ROW);
  window = row.indicesIn(Frame(19, 0, 2, 20));
  LOKA_VERIFY(window.first == 0 && window.count == 4);
  window = row.indicesIn(Frame(60, 0, 100, 20));
  LOKA_VERIFY(window.first == 6 && window.count == 1);
  const LazyLayout margin = FixedGrid(10, 20, 2, 7, STACK_AXIS_COLUMN, 1);
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
  const LazyLayout grid = FixedGrid(10, 20, 2, 7);
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
