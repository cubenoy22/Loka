#include "platform/file/FileIO.hpp"

#include <Files.h>
#include <Script.h>

namespace loka
{
  namespace platform
  {
    namespace file
    {
      namespace
      {
        /** Temporarily gives the C runtime an FSSpec's parent as its working
            folder, then restores the process-global folder before returning
            the opened stream to its caller. */
        class ToolboxWorkingFolderScope
        {
        public:
          ToolboxWorkingFolderScope()
              : oldVRefNum_(0),
                oldDirId_(0),
                active_(false)
          {
          }

          ~ToolboxWorkingFolderScope()
          {
            this->restore();
          }

          bool enter(short vRefNum, long dirId)
          {
            Str255 ignoredVolumeName;
            if (HGetVol(ignoredVolumeName, &this->oldVRefNum_, &this->oldDirId_) != noErr)
            {
              return false;
            }
            if (HSetVol(0, vRefNum, dirId) != noErr)
            {
              return false;
            }
            this->active_ = true;
            return true;
          }

          bool restore()
          {
            if (!this->active_)
            {
              return true;
            }
            if (HSetVol(0, this->oldVRefNum_, this->oldDirId_) != noErr)
            {
              return false;
            }
            this->active_ = false;
            return true;
          }

        private:
          short oldVRefNum_;
          long oldDirId_;
          bool active_;

          ToolboxWorkingFolderScope(const ToolboxWorkingFolderScope &);
          ToolboxWorkingFolderScope &operator=(const ToolboxWorkingFolderScope &);
        };
      } // namespace

      ReadResult ReadBytes(const FileHandle &handle,
                           std::vector<unsigned char> &out,
                           const ReadCapacity *capacity)
      {
        out.clear();
        if (!handle.hasSpec)
        {
          return READ_NO_NATIVE_SPEC;
        }
        short refNum = 0;
        OSErr err = FSpOpenDF(&handle.spec, fsRdPerm, &refNum);
        if (err != noErr)
        {
          return READ_NATIVE_OPEN_FAILED;
        }
        long fileSize = 0;
        err = GetEOF(refNum, &fileSize);
        if (err != noErr || fileSize < 0)
        {
          FSClose(refNum);
          return READ_NATIVE_SIZE_FAILED;
        }
        if (capacity && !capacity->allows(static_cast<std::size_t>(fileSize)))
        {
          FSClose(refNum);
          return READ_CAPACITY_REFUSED;
        }
        out.resize(static_cast<std::size_t>(fileSize));
        if (fileSize > 0)
        {
          long count = fileSize;
          err = FSRead(refNum, &count, &out[0]);
          if (err != noErr && err != eofErr)
          {
            FSClose(refNum);
            out.clear();
            return READ_NATIVE_READ_FAILED;
          }
          if (count < fileSize)
          {
            out.resize(static_cast<std::size_t>(count < 0 ? 0 : count));
          }
        }
        FSClose(refNum);
        return READ_OK;
      }

      PrepareResult PrepareTextDocumentDestination(const FileHandle &file)
      {
        if (!file.hasSpec)
          return PREPARE_NO_NATIVE_SPEC;
        // Numeric four-character codes avoid implementation-defined literals.
        const OSType textType = 0x54455854UL; // 'TEXT'
        const OSType simpleTextCreator = 0x74747874UL; // 'ttxt'
        FInfo info;
        const OSErr error = FSpGetFInfo(&file.spec, &info);
        if (error == fnfErr)
        {
          return FSpCreate(&file.spec, simpleTextCreator, textType, smSystemScript) == noErr
                     ? PREPARE_OK : PREPARE_CREATE_FAILED;
        }
        if (error != noErr)
          return PREPARE_CATALOG_FAILED;
        return info.fdType == textType ? PREPARE_OK : PREPARE_NOT_TEXT;
      }

      std::FILE *OpenWriteTruncate(const FileHandle &file)
      {
        if (!file.hasSpec || file.spec.name[0] == 0 || file.spec.name[0] > 63)
        {
          return 0;
        }

        ToolboxWorkingFolderScope folder;
        if (!folder.enter(file.spec.vRefNum, file.spec.parID))
        {
          return 0;
        }

        char name[64];
        const unsigned char length = file.spec.name[0];
        for (unsigned char i = 0; i < length; ++i)
        {
          name[i] = static_cast<char>(file.spec.name[i + 1]);
        }
        name[length] = '\0';

        std::FILE *result = std::fopen(name, "wb");
        if (!folder.restore())
        {
          if (result)
          {
            std::fclose(result);
          }
          return 0;
        }
        return result;
      }

      bool FlushWrite(std::FILE *stream, const FileHandle &file)
      {
        if (!stream || !file.hasSpec || std::fflush(stream) != 0)
        {
          return false;
        }
        return FlushVol(0, file.spec.vRefNum) == noErr;
      }
    } // namespace file
  } // namespace platform
} // namespace loka
