#ifndef LOKA_MAC_ATTRIBUTED_TEXT_CONTEXT_HPP
#define LOKA_MAC_ATTRIBUTED_TEXT_CONTEXT_HPP

#include "MacRetirableContext.hpp"
#include "app/nodes/AttributedText.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"

class MacScenePlatformController;

/** WHOLE_LINE projection; the completed attributed object belongs to this
    context, independently of AppKit's presented pixels. */
class MacAttributedTextContext : public MacRetirableContext
{
public:
  MacAttributedTextContext(MacScenePlatformController *controller,
                           void *parentView,
                           loka::app::AttributedTextNode *node);
  virtual ~MacAttributedTextContext();
  void readLifecycleFactOnAttach();
  virtual void onPropsApplied();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous, loka::app::scene::NodeLifecycleFact next);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

private:
  /** Null is unknown/refused; a completed object is immutable until clear. */
  class Projection
  {
  public:
    Projection();
    ~Projection();
    void clear();
    bool build(const loka::app::AttributedString &value,
               const loka::app::BlockStyle &block,
               const MacScenePlatformController &controller);
    void *value() const
    {
      return this->value_;
    }

  private:
    void *value_;
    Projection(const Projection &);
    Projection &operator=(const Projection &);
  };
  void clearProjection();
  loka::app::AttributedTextNode *node_;
  void *label_;
  Projection projection_;
};

void RegisterMacAttributedTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);
#endif
