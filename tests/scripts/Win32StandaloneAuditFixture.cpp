#include <windows.h>

int main()
{
  wchar_t source[32768];
  const DWORD length = GetEnvironmentVariableW(
      L"LOKA_STANDALONE_AUDIT_FIXTURE", source, sizeof(source) / sizeof(source[0]));
  if (length == 0 || length >= sizeof(source) / sizeof(source[0]))
  {
    return 2;
  }
  if (!CopyFileW(source, L"LOG.TXT", FALSE))
  {
    return 3;
  }
  Sleep(10000);
  return 0;
}
