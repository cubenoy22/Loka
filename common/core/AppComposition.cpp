#include "core/AppComposition.hpp"

AppComposition::AppComposition(PlatformContext *context)
    : components_(), windows_(), context_(context)
{
  assert(context_ && "AppComposition: PlatformContext* must not be nullptr");
}

AppComposition::~AppComposition()
{
  for (size_t i = 0; i < windows_.size(); ++i)
  {
    delete windows_[i];
  }
  windows_.clear();
}

AppComposition &AppComposition::declare(const WindowDefinitionBase &def)
{
  windows_.push_back(def.clone());
  return *this;
}

std::vector<AppComponent *> AppComposition::build()
{
  assert(context_ && "AppComposition::build requires PlatformContext");
  std::vector<AppComponent *> result = components_;
  for (size_t i = 0; i < windows_.size(); ++i)
  {
    class Window *window = windows_[i]->create(context_);
    if (window)
    {
      result.push_back(window);
    }
  }
  return result;
}

void AppComposition::setContext(PlatformContext *ctx)
{
  context_ = ctx;
}
