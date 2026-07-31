#include "LrpcPathTests.hpp"

#include <cassert>
#include <climits>
#include <cstdio>
#include <string>

#include "lrpc/Utf8Path.hpp"

void testLrpcConvertsManifestUtf8PathsToWide()
{
  const std::string utf8("Assets/\xE6\x97\xA5\xE6\x9C\xAC/\xF0\x9F\x8C\xB8.png");
  std::wstring expected(L"Assets/\x65E5\x672C/");
#if WCHAR_MAX <= 0xFFFF
  expected.push_back(static_cast<wchar_t>(0xD83C));
  expected.push_back(static_cast<wchar_t>(0xDF38));
#else
  expected.push_back(static_cast<wchar_t>(0x1F338));
#endif
  expected += L".png";

  std::wstring wide;
  assert(loka::lrpc::Utf8PathToWide(utf8, wide));
  assert(wide == expected);

  const char *invalid[] = {
      "\xC0\xAF",       // overlong slash
      "\xE2\x28\xA1", // non-continuation in a sequence
      "\xED\xA0\x80", // UTF-16 surrogate
      "\xF4\x90\x80\x80", // above U+10FFFF
      "\xF0\x9F\x8C"  // truncated sequence
  };
  for (std::size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
  {
    wide = L"preserved";
    assert(!loka::lrpc::Utf8PathToWide(invalid[i], wide));
    assert(wide == L"preserved");
  }

  std::printf("testLrpcConvertsManifestUtf8PathsToWide passed\n");
}
