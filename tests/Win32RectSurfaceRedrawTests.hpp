#ifndef LOKA_WIN32_RECT_SURFACE_REDRAW_TESTS_HPP
#define LOKA_WIN32_RECT_SURFACE_REDRAW_TESTS_HPP

void testWin32RectSurfaceTicksRepaintOnlySurface();
void testWin32PaintOnlyChangeUnderScrollViewKeepsSiblingPixels();

void testWin32PaintOnlyChangeUnderScrollViewKeepsSiblingPixels();
void testWin32RefusedAnswerKeepsBroadFallback();
void testWin32NonClearingSurfaceKeepsBroadFallback();
void testWin32PaintAnswerContracts();
void testWin32EditTextPaintDelivery();
void testWin32PopupMenuPaintDelivery();

void testWin32ZStackTextShowsSiblingBeneath();

#endif
