#ifndef LOKA_PLATFORM_FILE_FILEIO_HPP
#define LOKA_PLATFORM_FILE_FILEIO_HPP

#include <cstdio>
#include <vector>

#include "core/String.hpp"
#include "platform/file/FileHandle.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      /**
       * Opens a file for reading, naming it the way the platform names files.
       *
       * This is a seam rather than a call to std::fopen because flattening a
       * logical String to UTF-8 and handing the bytes to a narrow open is
       * wrong wherever the narrow API is code-page based. Windows decodes the
       * argument of fopen in the process ANSI code page, so UTF-8 bytes for a
       * path containing full-width characters name a file that does not exist
       * (#15). Targets whose narrow API already takes bytes keep using fopen,
       * which AGENTS.md deliberately prefers on Classic paths.
       *
       * The caller owns the returned handle and must fclose it.
       *
       * @param path Logical path to open. Its native form is materialized by
       *             the platform implementation; no conversion happens here.
       * @return An open read-only handle, or NULL if the file cannot be opened.
       */
      std::FILE *OpenRead(const loka::core::String &path);

      /** Result of a synchronous whole-file read. Native and stdio failures
          stay distinct so callers can preserve their own error vocabulary. */
      enum ReadResult
      {
        READ_OK,
        READ_NO_NATIVE_SPEC,
        READ_NATIVE_OPEN_FAILED,
        READ_NATIVE_SIZE_FAILED,
        READ_NATIVE_READ_FAILED,
        READ_STDIO_OPEN_FAILED,
        READ_STDIO_SEEK_FAILED,
        READ_STDIO_READ_FAILED,
        READ_CAPACITY_REFUSED,
        READ_SIZE_OVERFLOW
      };

      /** Borrowed admission policy, called synchronously before initial byte
          allocation and each chunk growth. It is never retained by a reader. */
      class ReadCapacity
      {
      public:
        virtual bool allows(std::size_t requiredBytes) const = 0;

      protected:
        ~ReadCapacity() {}
      };

      /** Reads a resolved file into caller-owned bytes, preserving native
          location data (Classic uses the FSSpec data fork). Clears out first;
          a failure leaves it empty. A null capacity policy imposes no limit.
          The reader closes its native/stdio handle on every exit. No fallback
          is implicit: callers choose whether to retry using a logical path. */
      ReadResult ReadBytes(const FileHandle &file, std::vector<unsigned char> &out,
                           const ReadCapacity *capacity = 0);

      /** Reads through OpenRead using the platform's native path encoding.
          Same byte ownership and capacity contract as the resolved overload;
          also serves callers explicitly retrying a resolved read via stdio. */
      ReadResult ReadBytes(const loka::core::String &path, std::vector<unsigned char> &out,
                           const ReadCapacity *capacity = 0);

      /** Outcome of preparing a resolved text destination before truncation. */
      enum PrepareResult
      {
        PREPARE_OK,
        PREPARE_NO_NATIVE_SPEC,
        PREPARE_NOT_TEXT,
        PREPARE_CATALOG_FAILED,
        PREPARE_CREATE_FAILED
      };

      /** Classic requires a native spec, preserves existing TEXT metadata and
          creates absent files as TEXT/ttxt using the system script. Other rails
          succeed without side effects; creation and permission errors remain
          OpenWriteTruncate's responsibility. Refusal must prevent writing. */
      PrepareResult PrepareTextDocumentDestination(const FileHandle &file);

      /** Opens an already platform-resolved file destination for binary
          write. This preserves native location data such as a Classic FSSpec. */
      std::FILE *OpenWriteTruncate(const FileHandle &file);

      /** Commits one stdio record through the platform's durability boundary.
          The stream remains open; false means the caller must fail closed. */
      bool FlushWrite(std::FILE *stream, const FileHandle &file);
    } // namespace file
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_FILE_FILEIO_HPP
