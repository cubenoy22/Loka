#include "ToolboxProfiler.hpp"
#include <Timer.h>
#include <Files.h>
#include <cstdio>
#include <cstring>

// Toolbox-specific result string
std::string gProfileResultString;

// Backend implementation using TickCount()
static long ToolboxGetTicks()
{
  return TickCount();
}

static loka::core::ProfilerBackend sToolboxBackend = {
    &ToolboxGetTicks};

void InitToolboxProfiler()
{
  loka::core::gProfilerBackend = &sToolboxBackend;
}

void BuildProfileResultString()
{
  gProfileResultString.clear();
  for (int i = 0; i < gProfileCount; i++)
  {
    char part[48];
    std::sprintf(part, "%s:%ld\n", gProfileData[i].name, gProfileData[i].ticks);
    gProfileResultString += part;
  }
  gProfileCount = 0;
}

// File-based output stream for function profiler
namespace
{
  struct FileProfileStream : public loka::core::ProfileOutputStream
  {
    short refNum;
    bool valid;

    FileProfileStream() : refNum(0), valid(false) {}

    bool open(const char *filename)
    {
      // Convert to Pascal string
      Str255 pName;
      size_t len = std::strlen(filename);
      if (len > 255)
      {
        len = 255;
      }
      pName[0] = static_cast<unsigned char>(len);
      std::memcpy(pName + 1, filename, len);

      // Use boot volume root (simple approach)
      short vRefNum = 0;
      long dirID = 0;

      // Create file (overwrite if exists)
      OSErr err = HCreate(vRefNum, dirID, pName, 'CWIE', 'TEXT');
      if (err != noErr && err != dupFNErr)
      {
        return false;
      }
      err = HOpen(vRefNum, dirID, pName, fsWrPerm, &refNum);
      if (err != noErr)
      {
        return false;
      }
      // Truncate existing file
      SetEOF(refNum, 0);
      valid = true;
      return true;
    }

    void close()
    {
      if (valid)
      {
        FSClose(refNum);
        valid = false;
      }
    }

    virtual void write(const char *data, int len)
    {
      if (!valid || len <= 0)
      {
        return;
      }
      long count = len;
      FSWrite(refNum, &count, data);
    }
  };
}

bool DumpFuncProfileToFile(const char *filename)
{
  if (!filename)
  {
    return false;
  }
  FileProfileStream stream;
  if (!stream.open(filename))
  {
    return false;
  }
  loka::core::DumpFuncProfileToStream(stream);
  stream.close();
  return true;
}
