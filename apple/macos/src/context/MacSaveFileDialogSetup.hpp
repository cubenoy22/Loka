#ifndef LOKA_MAC_SAVE_FILE_DIALOG_SETUP_HPP
#define LOKA_MAC_SAVE_FILE_DIALOG_SETUP_HPP

#include "app/OpenFileDialog.hpp"
#include "platform/StringUTF8.hpp"
#include "Utf8String.hpp"

namespace loka
{
  namespace macos
  {
    /** Native SAVE name projection; autoreleased/constant, or nil on conversion failure. */
    inline NSString *MacSaveFileDialogDefaultName(const app::FileDialogOptions &options)
    {
      std::string utf8;
      if (!platform::CollectUtf8(options.defaultName(), utf8))
        return nil;
      return CreateNSStringFromUtf8(utf8);
    }

    /** Native SAVE type suggestion; nil means unrestricted (not an empty array). */
    inline NSArray *MacSaveFileDialogAllowedTypes(const app::FileDialogOptions &options)
    {
      switch (options.filterPolicy())
      {
      case app::FILE_DIALOG_FILTER_DEFAULT:
        return nil;
      case app::FILE_DIALOG_FILTER_ALL_FILES_TEXT:
        return [NSArray arrayWithObject:@"txt"];
      }
      return nil;
    }
  }
}

#endif
