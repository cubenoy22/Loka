#ifndef LOKA_MAC_SCROLL_VIEW_CONTEXT_HPP
#define LOKA_MAC_SCROLL_VIEW_CONTEXT_HPP

#include "MacRetirableContext.hpp"
#include "app/nodes/nestable/ScrollView.hpp"

class MacScenePlatformController;

/** Owns the NSScrollView that clips a ScrollView subtree and whose flipped
    document view is the native projection parent for that subtree. */
class MacScrollViewContext : public MacRetirableContext
{
public:
  MacScrollViewContext(MacScenePlatformController *controller,
                       void *parentView,
                       int x,
                       int y,
                       int width,
                       int height,
                       loka::app::ScrollViewNode *node);
  virtual ~MacScrollViewContext();

  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous, loka::app::scene::NodeLifecycleFact next);

  bool isValid() const;
  void *documentView() const;
  int contentWidth() const;
  void relayout(int x, int y, int width, int height);

  /** Sets the measured document height and native position, returning the
      exact logical offset represented after clamping. */
  int setScrollMetrics(int contentHeight, int viewportHeight, int offset);

  /** Notification door used by the owned NSScrollView observer. */
  void publishClipViewBoundsOrigin();

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  int clampOffset(int value, int maximum) const;
  int maximumOffset() const;
  void publishOffset(int value);

  loka::app::ScrollViewNode *node_;
  void *scrollView_;
};

#endif // LOKA_MAC_SCROLL_VIEW_CONTEXT_HPP
