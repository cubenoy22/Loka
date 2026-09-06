#ifndef LOKA_TESTS_PLATFORM_NULL_SCROLL_BAR_CONTEXT_HPP
#define LOKA_TESTS_PLATFORM_NULL_SCROLL_BAR_CONTEXT_HPP

#include "app/nodes/controls/ScrollBar.hpp"
#include "app/scene/projection/NativeNodeContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"

/** The null arm's stand-in for a scrollBarProc control. It carries the one
    piece of behaviour the contract is about: a tracked value the user moves
    freely, and a single settle point where that value crosses into Loka.
    The Toolbox arm's tracking loop is TrackControl; here it is the
    simulate* seams, so the ruling can be asserted without an OS. */
class NullScrollBarContext : public loka::app::scene::NativeNodeContext
{
public:
  enum Part
  {
    PART_LINE_UP,
    PART_LINE_DOWN,
    PART_PAGE_UP,
    PART_PAGE_DOWN
  };

  NullScrollBarContext(loka::app::ScrollBarNode *node, NullScenePlatformController *controller);
  virtual ~NullScrollBarContext();

  /** Attach-time read (late-subscriber rule): presentation from the
      current fact, called by the installing handler after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  virtual short layout(loka::app::scene::IPlatformController *controller,
                       loka::app::scene::LayoutState &state);

  /** Re-reads the declared range and the bound value into the native side.
      Driven from every projection so a recomposed range reaches the control
      the same way SetControlMinimum/Maximum does on Toolbox. */
  void syncFromNode();

  int displayedValue() const;
  int minimum() const;
  int maximum() const;
  int lineStep() const;
  int pageStep() const;
  loka::app::ScrollBarOrientation orientation() const;
  /** False when disabled or when the range cannot scroll -- the hilite 255
      presentation, and the reason a click does nothing. */
  bool active() const;
  /** Number of times a settled value crossed into the bound State. The
      ruling is that a held arrow produces exactly one, not one per tick. */
  unsigned long stateWriteCount() const;

  /** One complete press: `repeatWhileHeld` action-proc ticks advancing what
      the user sees, then the release that settles the value. */
  void simulatePress(Part part, int repeatWhileHeld);
  /** One complete thumb drag: the outline lands on `value`, then release. */
  void simulateThumbDragTo(int value);

  /** The same gesture, split at the joint the ruling is about: ticks move
      only what the user sees, release is the single settle point. Exposed
      separately so a test can assert nothing crossed into Loka *between*
      them -- the completed-gesture seams above cannot observe that. */
  void pressTick(Part part);
  void dragThumbTo(int value);
  void release();

private:
  void commitTrackedValue();

  loka::app::ScrollBarNode *node_;
  NullScenePlatformController *controller_;
  NullScenePlatformController::FakeControlHandle *handle_;
  int trackedValue_;
  /** What the binding last pushed into the control (the Toolbox arm's
      `appliedValue`). A gesture that settles back on it -- a cancelled drag,
      an arrow held against the end of the range -- publishes nothing. */
  int appliedValue_;
  unsigned long stateWriteCount_;
};

void RegisterNullScrollBarNodeHandler(NullScenePlatformController &controller);

#endif // LOKA_TESTS_PLATFORM_NULL_SCROLL_BAR_CONTEXT_HPP
