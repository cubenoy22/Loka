#include "ToolboxPlatformContext.hpp"
#include "ToolboxFileChoice.hpp"
#include "platform/ToolboxPascalText.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/file/FileLocatorAccess.hpp"

bool ToolboxCaptureChosenFile(const FSSpec &spec, loka::file::File &out)
{
  const unsigned length = spec.name[0];
  if (!length || length > 63)
    return false;
  unsigned char bytes[70];
  const unsigned short volume = static_cast<unsigned short>(spec.vRefNum);
  const unsigned long parent = static_cast<unsigned long>(spec.parID);
  bytes[0] = static_cast<unsigned char>(volume >> 8);
  bytes[1] = static_cast<unsigned char>(volume);
  bytes[2] = static_cast<unsigned char>(parent >> 24);
  bytes[3] = static_cast<unsigned char>(parent >> 16);
  bytes[4] = static_cast<unsigned char>(parent >> 8);
  bytes[5] = static_cast<unsigned char>(parent);
  bytes[6] = static_cast<unsigned char>(length);
  for (unsigned i = 0; i < length; ++i)
    bytes[7 + i] = spec.name[1 + i];
  return loka::platform::file::FileLocatorAccess::capture(
      ToolboxChosenFileDisplayName(spec.name + 1, length), loka::file::File::KIND_FILE,
      bytes, 7 + length, out);
}

bool QueryToolboxSpec(const loka::file::File &file, FSSpec &out)
{
  const unsigned char *bytes = 0;
  std::size_t size = 0;
  if (!loka::platform::file::FileLocatorAccess::query(file, bytes, size)
      || size < 7 || !bytes[6] || bytes[6] > 63 || size != 7u + bytes[6])
    return false;
  FSSpec spec = {};
  const unsigned volume = (static_cast<unsigned>(bytes[0]) << 8) | bytes[1];
  const unsigned long parent = (static_cast<unsigned long>(bytes[2]) << 24)
      | (static_cast<unsigned long>(bytes[3]) << 16)
      | (static_cast<unsigned long>(bytes[4]) << 8) | bytes[5];
  // Decode signed two's-complement fields without an out-of-range signed cast.
  spec.vRefNum = static_cast<short>(volume <= 0x7FFFu ? static_cast<long>(volume)
      : -1L - static_cast<long>(0xFFFFu - volume));
  spec.parID = parent <= 0x7FFFFFFFUL ? static_cast<long>(parent)
      : -1L - static_cast<long>(0xFFFFFFFFUL - parent);
  spec.name[0] = bytes[6];
  for (unsigned i = 0; i < bytes[6]; ++i)
    spec.name[1 + i] = bytes[7 + i];
  out = spec;
  return true;
}

bool ToolboxPlatformContext::openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const
{
  out = loka::platform::file::FileHandle();
  if (item.base() == loka::file::File::BASE_REFUSED)
    return false;
  if (!item.locator().empty())
  {
    if (!QueryToolboxSpec(item, out.spec))
      return false;
    out.hasSpec = true;
    out.displayPath = item.toString();
    out.kind = item.kind();
    return true;
  }
  if (item.base() == loka::file::File::BASE_APPLICATION)
    return loka::platform::file::ResolveApplicationItem(item, out);
  out.displayPath = item.toString();
  out.kind = item.kind();
  return !out.displayPath.empty();
}
