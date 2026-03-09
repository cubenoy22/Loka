#ifndef LOKA_DSL_SNAP_FORMAT_HPP
#define LOKA_DSL_SNAP_FORMAT_HPP

#include <string>
#include <vector>

namespace loka
{
  namespace dsl
  {
    enum SnapWriteStatus
    {
      SNAP_WRITE_OK = 0,
      SNAP_WRITE_INVALID_PATH = 1,
      SNAP_WRITE_LIMIT_EXCEEDED = 2,
      SNAP_WRITE_IO_ERROR = 3
    };

    struct SnapKeyValue
    {
      std::string key;
      std::string value;
    };

    class SnapRecord
    {
    public:
      void set(const char *key, const char *value);
      void setInt(const char *key, long value);
      bool has(const char *key) const;
      bool validateV1RequiredKeys(std::string &missingKey) const;
      std::string serialize(bool appendBlankLine) const;

    private:
      static std::string escapeValue(const std::string &value);
      std::vector<SnapKeyValue> entries_;
    };

    class SnapFileWriter
    {
    public:
      static SnapWriteStatus appendRecordStatus(const char *path, const SnapRecord &record);
      static SnapWriteStatus appendRecordStatusWithLimits(const char *path,
                                                          const SnapRecord &record,
                                                          long maxTotalBytes,
                                                          long maxRecords);
      static bool appendRecord(const char *path, const SnapRecord &record);
      static bool appendRecordWithMaxBytes(const char *path, const SnapRecord &record, long maxTotalBytes);
      static bool appendRecordWithLimits(const char *path,
                                         const SnapRecord &record,
                                         long maxTotalBytes,
                                         long maxRecords);
    };

    class SnapTestConfig
    {
    public:
      struct Settings
      {
        Settings()
            : captureDir(),
              hasCaptureDir(false),
              maxFiles(0),
              hasMaxFiles(false),
              maxTotalBytes(0),
              hasMaxTotalBytes(false) {}

        std::string captureDir;
        bool hasCaptureDir;
        long maxFiles;
        bool hasMaxFiles;
        long maxTotalBytes;
        bool hasMaxTotalBytes;
      };

      static bool load(const char *configPath, Settings &out);

      // Resolves relative output path with capture_dir in LokaTest.cfg.
      // Returns original path when config is missing/invalid or path is absolute.
      static std::string resolveCapturePath(const char *path, const char *configPath = "LokaTest.cfg");
    };
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_SNAP_FORMAT_HPP
