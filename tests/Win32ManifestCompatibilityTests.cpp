#include "Win32ManifestCompatibilityTests.hpp"

#include "support/TestVerify.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <VersionHelpers.h>

#include <cstdio>
#include <cstring>

namespace
{
  typedef LONG(WINAPI *RtlGetVersionFn)(PRTL_OSVERSIONINFOW);

  // RtlGetVersion reports the kernel's version regardless of what the
  // executable's manifest declares; VerifyVersionInfo (behind the
  // VersionHelpers inline functions) answers from the manifest's supportedOS
  // list and caps at 6.2 when the running OS is not listed.
  bool readKernelVersion(RTL_OSVERSIONINFOW &version)
  {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
    {
      return false;
    }
    RtlGetVersionFn rtlGetVersion =
        reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtlGetVersion)
    {
      return false;
    }
    std::memset(&version, 0, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    return rtlGetVersion(&version) == 0;
  }
} // namespace

void testWin32ManifestDeclaresTheRunningWindowsVersion()
{
  RTL_OSVERSIONINFOW kernel;
  LOKA_VERIFY(readKernelVersion(kernel));
  const bool kernelIs8Point1OrGreater =
      kernel.dwMajorVersion > 6 || (kernel.dwMajorVersion == 6 && kernel.dwMinorVersion >= 3);
  const bool kernelIs10OrGreater = kernel.dwMajorVersion >= 10;
  printf("  kernel=%lu.%lu.%lu IsWindows8Point1OrGreater=%d IsWindows10OrGreater=%d\n",
         static_cast<unsigned long>(kernel.dwMajorVersion),
         static_cast<unsigned long>(kernel.dwMinorVersion),
         static_cast<unsigned long>(kernel.dwBuildNumber),
         IsWindows8Point1OrGreater() ? 1 : 0,
         IsWindows10OrGreater() ? 1 : 0);
  fflush(stdout);
  // Red without the supportedOS list in cmake/LokaWin32VisualStyles.manifest:
  // on a Windows 10 or 11 host both helpers then answer false, because the
  // compatibility query is capped at Windows 8 for an executable that does
  // not declare the running version.
  LOKA_VERIFY(IsWindows8Point1OrGreater() == kernelIs8Point1OrGreater &&
              "the manifest must declare Windows 8.1 so VerifyVersionInfo sees it");
  LOKA_VERIFY(IsWindows10OrGreater() == kernelIs10OrGreater &&
              "the manifest must declare Windows 10 so VerifyVersionInfo sees it");
}
