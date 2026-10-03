#include "ToolboxPlatformContext.hpp"

#include "platform/file/AppLocation.hpp"
#include "platform/file/FileHandle.hpp"
#include <vector>

namespace
{
  struct SpecBinding
  {
    loka::core::String displayPath;
    FSSpec spec;
  };

  static std::vector<SpecBinding> gChosenSpecs;

  static bool FindChosenSpec(const loka::core::String &displayPath, FSSpec &specOut)
  {
    for (std::size_t i = 0; i < gChosenSpecs.size(); ++i)
    {
      if (gChosenSpecs[i].displayPath.equals(displayPath))
      {
        specOut = gChosenSpecs[i].spec;
        return true;
      }
    }
    return false;
  }
} // namespace

bool ToolboxPlatformContext::openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const
{
  // Keep this refusal before all resolution, as on the other platform rails.
  if (item.base() == loka::file::File::BASE_REFUSED)
  {
    out = loka::platform::file::FileHandle();
    return false;
  }
  if (item.base() == loka::file::File::BASE_APPLICATION)
  {
    return loka::platform::file::ResolveApplicationItem(item, out);
  }
  out.displayPath = item.toString();
  out.kind = item.kind();
#if defined(LOKA_RETRO68)
  out.hasSpec = false;
  FSSpec spec;
  if (FindChosenSpec(out.displayPath, spec))
  {
    out.spec = spec;
    out.hasSpec = true;
  }
#endif
  return !out.displayPath.empty();
}

#if defined(LOKA_RETRO68)
void ToolboxPlatformContext::registerChosenFileSpec(const loka::core::String &displayPath, const FSSpec &spec)
{
  for (std::size_t i = 0; i < gChosenSpecs.size(); ++i)
  {
    if (gChosenSpecs[i].displayPath.equals(displayPath))
    {
      gChosenSpecs[i].spec = spec;
      return;
    }
  }
  SpecBinding binding;
  binding.displayPath = displayPath;
  binding.spec = spec;
  gChosenSpecs.push_back(binding);
}
#endif
