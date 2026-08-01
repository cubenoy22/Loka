#ifndef LOKA_TESTS_PLATFORM_NULL_TEXT_CONTEXT_HPP
#define LOKA_TESTS_PLATFORM_NULL_TEXT_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
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

class NullTextContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit NullTextContext(loka::app::TextNode *node);
  virtual ~NullTextContext();

  void readLifecycleFactOnAttach();

  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

  const NullTextMeasurement &measurement() const;

private:
  loka::app::TextNode *node_;
  NullTextMeasurement measurement_;
};

void RegisterNullTextNodeHandler(NullScenePlatformController &controller);

#endif // LOKA_TESTS_PLATFORM_NULL_TEXT_CONTEXT_HPP
