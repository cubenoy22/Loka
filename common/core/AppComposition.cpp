#include "core/AppComposition.hpp"

AppComposition::AppComposition(PlatformContext *context)
    : components_(), windowList_(), context_(context)
{
  assert(context_ && "AppComposition: PlatformContext* must not be null");
}

AppComposition::~AppComposition()
{
  windowList_.clear();
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
  if (windowList_.count() == 0)
  {
    return result;
  }
  loka::dsl::CompositionCursor<WindowDefinitionBase> it(windowList_.head(), windowList_.count());
  for (WindowDefinitionBase *def = it.next(); def; def = it.next())
  {
    class Window *window = def->create(context_);
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
