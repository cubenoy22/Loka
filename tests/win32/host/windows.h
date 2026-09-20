#ifndef LOKA_TEST_GDI_WINDOWS_H
#define LOKA_TEST_GDI_WINDOWS_H
#include <cstddef>
#include <string>
#include <vector>
typedef int BOOL;
typedef unsigned int UINT;
// Host wchar_t holds the same UTF-16 unit values; this fixture is not an ABI test.
typedef wchar_t WCHAR;
struct HostFont
{
  int ascent, descent, leading, advance;
};
typedef HostFont *HFONT;
typedef void *HGDIOBJ;
struct HostDC
{
  HFONT font;
  UINT alignment;
  int background;
};
typedef HostDC *HDC;
struct RECT
{
  int left, top, right, bottom;
};
struct SIZE
{
  int cx, cy;
};
struct TEXTMETRICW
{
  int tmAscent, tmDescent, tmExternalLeading;
};
#define FALSE 0
#define TRUE 1
#define OBJ_FONT 6
#define HGDI_ERROR reinterpret_cast<HGDIOBJ>(-1)
#define TRANSPARENT 1
#define TA_BASELINE 24
#define TA_LEFT 0
#define ETO_CLIPPED 4
HGDIOBJ GetCurrentObject(HDC, UINT);
HGDIOBJ SelectObject(HDC, HGDIOBJ);
BOOL GetTextMetricsW(HDC, TEXTMETRICW *);
BOOL GetTextExtentExPointW(HDC, const WCHAR *, int, int, int *, int *, SIZE *);
UINT GetTextAlign(HDC);
UINT SetTextAlign(HDC, UINT);
int SetBkMode(HDC, int);
BOOL ExtTextOutW(HDC, int, int, UINT, const RECT *, const WCHAR *, UINT, const int *);
namespace win32_host
{
  struct Draw
  {
    int x, y;
    HFONT font;
    std::wstring units;
  };
  extern int selections, measures, metrics;
  extern bool failMeasure;
  extern std::vector<Draw> draws;
  void reset();
} // namespace win32_host
#endif
