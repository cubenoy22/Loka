#include "core/AppComposition.hpp"

AppComposition::AppComposition(PlatformContext *context)
    : components_(), windows_(), windowList_(), context_(context)
{
  assert(context_ && "AppComposition: PlatformContext* must not be null");
}

AppComposition::~AppComposition()
{
  windowList_.clear();
  for (size_t i = 0; i < windows_.size(); ++i)
  {
    delete windows_[i];
  }
  windows_.clear();
}

AppComposition &AppComposition::declare(const WindowDefinitionBase &def)
{
  windowList_.appendClone(def);
  return *this;
}

AppComposition &AppComposition::declare(const WindowDefinitionBase *def)
{
  if (!def)
  {
    return *this;
  }
  return declare(*def);
}

AppComposition &AppComposition::operator<<(const WindowDefinitionBase &def)
{
  return declare(def);
}

AppComposition &AppComposition::operator<<(const WindowDefinitionBase *def)
{
  return declare(def);
}

std::vector<AppComponent *> AppComposition::build()
{
  assert(context_ && "AppComposition::build requires PlatformContext");
  std::vector<AppComponent *> result = components_;
  if (windows_.empty() && windowList_.count() > 0)
  {
    windowList_.detachTo(windows_);
  }
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
