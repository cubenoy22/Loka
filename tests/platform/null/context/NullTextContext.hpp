#ifndef LOKA_TESTS_PLATFORM_NULL_TEXT_CONTEXT_HPP
#define LOKA_TESTS_PLATFORM_NULL_TEXT_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/nodes/Text.hpp"
#include "platform/null/context/NullPaintPlacement.hpp"
#include "platform/null/NullScenePlatformController.hpp"

namespace loka
{
  namespace app
  {
    class TextNode;
  }
} // namespace loka

/** Completed geometry from one deterministic null-platform text measure. */
class NullTextMeasurement
{
public:
  NullTextMeasurement();
  NullTextMeasurement(short width, short height, short lineCount);

  short width() const;
  short height() const;
  short lineCount() const;

private:
  short width_;
  short height_;
  short lineCount_;
};

/** Resolved, pointer-free layout inputs; live font state is read when captured. */
struct NullTextPaintStyle
{
  NullTextPaintStyle()
      : fontSize(0),
        weight(loka::app::TEXT_WEIGHT_NORMAL),
        wrap(loka::app::TEXT_WRAP_NONE),
        truncation(loka::app::TEXT_TRUNCATION_NONE)
  {
  }
  explicit NullTextPaintStyle(const loka::app::TextProps &props);
  bool operator==(const NullTextPaintStyle &other) const
  {
    return fontSize == other.fontSize && weight == other.weight && wrap == other.wrap && truncation == other.truncation;
  }
  int fontSize;
  loka::app::TextWeight weight;
  loka::app::TextWrap wrap;
  loka::app::TextTruncation truncation;
};

class NullTextContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit NullTextContext(loka::app::TextNode *node);
  virtual ~NullTextContext();

  void readLifecycleFactOnAttach();

  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

  const NullTextMeasurement &measurement() const;

  virtual loka::app::scene::PaintAnswer queryPaintDamage(const loka::app::scene::PaintQuery &query) const;
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous, loka::app::scene::NodeLifecycleFact next);
  using loka::app::scene::NativeNodeContext::commitPresented;
  bool commitPresented(const loka::core::String &value, const loka::app::scene::PaintScope &scope);
  void invalidatePaintHistory()
  {
    this->presented_.invalidate();
  }
  /** Called before every fallible projection: a placement is a derived cache and is
      re-established only by a successful layout in that pass (AGENTS.md
      failure-degradation). A refused projection therefore leaves no stale seat. */
  void invalidatePresentation()
  {
    this->presented_.invalidate();
    this->placement_.invalidate();
  }

private:
  loka::app::scene::PaintFact<loka::core::String> presented_;
  NullPaintPlacement placement_;
  NullTextPaintStyle placedStyle_;
  loka::app::TextNode *node_;
  NullTextMeasurement measurement_;
};

void RegisterNullTextNodeHandler(NullScenePlatformController &controller);

#endif // LOKA_TESTS_PLATFORM_NULL_TEXT_CONTEXT_HPP
