#ifndef LOKA_DSL_SNAP_FORMAT_HPP
#define LOKA_DSL_SNAP_FORMAT_HPP

#include <string>
#include <vector>

namespace loka
{
  namespace dsl
  {
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
      static bool appendRecord(const char *path, const SnapRecord &record);
    };
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_SNAP_FORMAT_HPP
