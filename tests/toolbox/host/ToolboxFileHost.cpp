#include "ToolboxFileHost.hpp"
#include "Processes.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>

namespace
{
  // Counted names include embedded zeroes; unused Str63 tails are not identity.
  struct Address
  {
    short volume;
    int32_t parent;
    std::string name;
    explicit Address(const FSSpec &spec)
        : volume(spec.vRefNum), parent(spec.parID),
          name(reinterpret_cast<const char *>(spec.name + 1), spec.name[0]) {}
    bool operator<(const Address &other) const
    {
      if (volume != other.volume) return volume < other.volume;
      if (parent != other.parent) return parent < other.parent;
      return name < other.name;
    }
  };
  struct Reader
  {
    std::string bytes;
    std::size_t position;
    explicit Reader(const std::string &value) : bytes(value), position(0) {}
  };
  std::map<Address, std::string> files;
  std::map<short, Reader> readers;
  FSSpec application;
  toolbox_file_host::ReadFailure readFailure = toolbox_file_host::NoFailure;
}
namespace toolbox_file_host
{
  FSSpec Spec(short volume, int32_t parent, const std::string &name)
  {
    assert(name.size() <= 63);
    FSSpec result = {};
    result.vRefNum = volume;
    result.parID = parent;
    result.name[0] = static_cast<unsigned char>(name.size());
    std::memcpy(result.name + 1, name.data(), name.size());
    return result;
  }
  void Put(const FSSpec &spec, const std::string &contents)
  {
    assert(spec.name[0] <= 63);
    files[Address(spec)] = contents;
  }
  void Remove(const FSSpec &spec) { files.erase(Address(spec)); }
  void FailRead(ReadFailure failure) { readFailure = failure; }
  void SetApplication(const FSSpec &spec) { application = spec; }
  std::size_t OpenCount() { return readers.size(); }
}
OSErr FSpOpenDF(const FSSpec *spec, signed char permission, short *out)
{
  if (!spec || !out || spec->name[0] > 63 || permission != fsRdPerm) return paramErr;
  const std::map<Address, std::string>::const_iterator found = files.find(Address(*spec));
  if (found == files.end()) return fnfErr;
  short ref = 1;
  while (readers.find(ref) != readers.end())
  {
    if (ref == 32767) return paramErr;
    ++ref;
  }
  readers.insert(std::make_pair(ref, Reader(found->second)));
  *out = ref;
  return noErr;
}
OSErr FSClose(short ref) { return readers.erase(ref) == 1 ? noErr : paramErr; }
OSErr GetEOF(short ref, long *out)
{
  if (readFailure == toolbox_file_host::SizeFailure) return paramErr;
  const std::map<short, Reader>::const_iterator found = readers.find(ref);
  if (found == readers.end() || !out) return paramErr;
  *out = static_cast<long>(found->second.bytes.size());
  return noErr;
}
OSErr SetFPos(short ref, short mode, long position)
{
  const std::map<short, Reader>::iterator found = readers.find(ref);
  if (found == readers.end() || mode != fsFromStart || position < 0 || position > INT32_MAX) return paramErr;
  found->second.position = static_cast<std::size_t>(position);
  return noErr;
}
OSErr FSRead(short ref, long *count, void *out)
{
  if (readFailure == toolbox_file_host::DataFailure) return paramErr;
  const std::map<short, Reader>::iterator found = readers.find(ref);
  if (found == readers.end() || !count || *count < 0 || *count > INT32_MAX || (!out && *count)) return paramErr;
  Reader &reader = found->second;
  const std::size_t remaining = reader.position < reader.bytes.size() ? reader.bytes.size() - reader.position : 0;
  const std::size_t n = std::min(static_cast<std::size_t>(*count), remaining);
  if (n) std::memcpy(out, reader.bytes.data() + reader.position, n);
  const bool complete = n == static_cast<std::size_t>(*count);
  reader.position += n;
  *count = static_cast<long>(n);
  return complete ? noErr : eofErr;
}
OSErr FSMakeFSSpec(short volume, long parent, const unsigned char *name, FSSpec *out)
{
  if (!name || !out || name[0] > 63 || parent < INT32_MIN || parent > INT32_MAX) return paramErr;
  *out = toolbox_file_host::Spec(volume, static_cast<int32_t>(parent),
      std::string(reinterpret_cast<const char *>(name + 1), name[0]));
  return files.find(Address(*out)) == files.end() ? fnfErr : noErr;
}
OSErr GetCurrentProcess(ProcessSerialNumber *out)
{
  out->highLongOfPSN = 0;
  out->lowLongOfPSN = 1;
  return noErr;
}
OSErr GetProcessInformation(const ProcessSerialNumber *, ProcessInfoRec *out)
{
  *out->processAppSpec = application;
  out->processName[0] = 0;
  return noErr;
}
// Write-side APIs are deliberately unsupported by this read-only fixture.
OSErr HGetVol(unsigned char *, short *, long *) { return paramErr; }
OSErr HSetVol(const unsigned char *, short, long) { return paramErr; }
OSErr FlushVol(const unsigned char *, short) { return paramErr; }

namespace toolbox_host { long systemScript = 0; unsigned scriptReads = 0; }
long GetScriptManagerVariable(short)
{
  ++toolbox_host::scriptReads;
  return toolbox_host::systemScript;
}
