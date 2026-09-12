#ifndef LOKA_TEST_WIN32_DIALOG_RESULT_ACCESS_HPP
#define LOKA_TEST_WIN32_DIALOG_RESULT_ACCESS_HPP
#include "app/OpenFileDialog.hpp"
class Win32Window;
class Win32OpenFileDialogContext;
/** Bounded pause seam after native-equivalent capture, before wake dispatch. */
class Win32DialogResultTestAccess
{
public:
  static Win32OpenFileDialogContext *create(Win32Window &window, loka::app::OpenFileDialogNode *node);
  static void queue(Win32OpenFileDialogContext &context, const loka::app::FileChooserResult &result);
};
#endif
