#include "Win32FilePathTests.hpp"

#include <windows.h>

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "core/String.hpp"
#include "platform/StringUTF8.hpp"
#include "platform/file/FileIO.hpp"

namespace
{
  // "テスト" and "画像.bin" — the payload shape from #15. Their UTF-8 bytes are
  // not a valid CP932 path, which is precisely why the pre-fix narrow open fails.
  const wchar_t kDirNameWide[] = {0x30C6, 0x30B9, 0x30C8, 0};                       // テスト
  const wchar_t kFileNameWide[] = {0x753B, 0x50CF, L'.', L'b', L'i', L'n', 0};      // 画像.bin

  const unsigned char kPayload[] = {0x4C, 0x4F, 0x4B, 0x41};

  bool WideToUtf8(const std::wstring &wide, std::string &out)
  {
    out.clear();
    if (wide.empty())
      return true;
    const int needed =
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.length()), NULL, 0, NULL, NULL);
    if (needed <= 0)
      return false;
    out.resize(static_cast<std::size_t>(needed));
    const int written = WideCharToMultiByte(
        CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.length()), &out[0], needed, NULL, NULL);
    return written == needed;
  }
} // namespace

void testWin32OpenReadAcceptsFullWidthPath()
{
  printf("\n==== [testWin32OpenReadAcceptsFullWidthPath] start ====\n");

  wchar_t tempDir[MAX_PATH];
  const DWORD tempLen = GetTempPathW(MAX_PATH, tempDir);
  assert(tempLen > 0 && tempLen < MAX_PATH);

  std::wstring dirWide(tempDir, tempDir + tempLen);
  dirWide += L"loka-fw15-";
  dirWide += kDirNameWide;
  RemoveDirectoryW(dirWide.c_str());
  const BOOL made = CreateDirectoryW(dirWide.c_str(), NULL);
  assert((made || GetLastError() == ERROR_ALREADY_EXISTS) && "could not create the full-width fixture directory");

  std::wstring fileWide = dirWide;
  fileWide += L"\\";
  fileWide += kFileNameWide;

  FILE *seed = _wfopen(fileWide.c_str(), L"wb");
  assert(seed && "fixture write must use the wide CRT: the narrow one is the thing under test");
  assert(fwrite(kPayload, 1, sizeof(kPayload), seed) == sizeof(kPayload));
  fclose(seed);

  std::string utf8Path;
  assert(WideToUtf8(fileWide, utf8Path));
  const loka::core::String logicalPath((std::string(utf8Path)));

  // The fix: the seam names the file the way Windows names files.
  FILE *opened = loka::platform::file::OpenRead(logicalPath);
  assert(opened && "OpenRead must reach a path containing full-width characters (#15)");
  unsigned char read[sizeof(kPayload)] = {0};
  assert(fread(read, 1, sizeof(read), opened) == sizeof(read));
  fclose(opened);
  for (std::size_t i = 0; i < sizeof(kPayload); ++i)
  {
    assert(read[i] == kPayload[i] && "OpenRead must return the bytes of the file that was written");
  }

  // Documented pre-fix failure shape, kept as a contrast pin: flattening the
  // logical path to UTF-8 and handing the bytes to the narrow CRT open is what
  // BlobLoader and SimpleViewer used to do, and it cannot reach this file
  // because fopen decodes its argument in the process ANSI code page. If this
  // ever succeeds, either the machine is running the UTF-8 code page (excluded
  // below) or someone has re-introduced the bug's premise and this pin is stale.
  if (GetACP() != CP_UTF8)
  {
    std::string flattened;
    assert(loka::platform::CollectUtf8(logicalPath, flattened));
    assert(flattened == utf8Path && "the logical path must survive as UTF-8; the defect is in the open, not the String");
    FILE *narrow = std::fopen(flattened.c_str(), "rb");
    assert(!narrow && "the narrow open must still fail on a full-width path; that is why the seam exists");
    if (narrow)
      fclose(narrow);
  }
  else
  {
    printf("  [skip] ACP is UTF-8, so the narrow-open contrast pin cannot discriminate on this machine\n");
  }

  DeleteFileW(fileWide.c_str());
  RemoveDirectoryW(dirWide.c_str());

  printf("==== [testWin32OpenReadAcceptsFullWidthPath] end ====\n");
}
