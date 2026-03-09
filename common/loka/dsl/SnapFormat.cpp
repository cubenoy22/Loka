#include "loka/dsl/SnapFormat.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>

namespace loka
{
  namespace dsl
  {
    namespace
    {
      static bool keyEquals(const std::string &lhs, const char *rhs)
      {
        return rhs ? lhs == rhs : false;
      }

      static bool keyLess(const SnapKeyValue &lhs, const SnapKeyValue &rhs)
      {
        return lhs.key < rhs.key;
      }
    } // namespace

    void SnapRecord::set(const char *key, const char *value)
    {
      assert(key && "SnapRecord::set requires key");
      if (!key)
      {
        return;
      }
      const std::string keyStr(key);
      for (size_t i = 0; i < entries_.size(); ++i)
      {
        if (entries_[i].key == keyStr)
        {
          entries_[i].value = value ? value : "";
          return;
        }
      }
      SnapKeyValue kv;
      kv.key = keyStr;
      kv.value = value ? value : "";
      entries_.push_back(kv);
    }

    void SnapRecord::setInt(const char *key, long value)
    {
      std::ostringstream oss;
      oss << value;
      set(key, oss.str().c_str());
    }

    bool SnapRecord::has(const char *key) const
    {
      for (size_t i = 0; i < entries_.size(); ++i)
      {
        if (keyEquals(entries_[i].key, key))
        {
          return true;
        }
      }
      return false;
    }

    bool SnapRecord::validateV1RequiredKeys(std::string &missingKey) const
    {
      static const char *kRequired[] = {
          "format_version",
          "schema_version",
          "scenario_version",
          "test",
          "step",
          "node",
          "tick",
          "status"};
      const size_t kRequiredCount = sizeof(kRequired) / sizeof(kRequired[0]);
      for (size_t i = 0; i < kRequiredCount; ++i)
      {
        if (!has(kRequired[i]))
        {
          missingKey = kRequired[i];
          return false;
        }
      }
      missingKey.clear();
      return true;
    }

    std::string SnapRecord::serialize(bool appendBlankLine) const
    {
      std::vector<SnapKeyValue> sorted = entries_;
      std::sort(sorted.begin(), sorted.end(), &keyLess);
      std::string out;
      for (size_t i = 0; i < sorted.size(); ++i)
      {
        out += sorted[i].key;
        out += '\t';
        out += escapeValue(sorted[i].value);
        out += '\n';
      }
      if (appendBlankLine)
      {
        out += '\n';
      }
      return out;
    }

    std::string SnapRecord::escapeValue(const std::string &value)
    {
      std::string escaped;
      escaped.reserve(value.size());
      for (size_t i = 0; i < value.size(); ++i)
      {
        const char c = value[i];
        if (c == '\\')
        {
          escaped += "\\\\";
        }
        else if (c == '\t')
        {
          escaped += "\\t";
        }
        else if (c == '\n')
        {
          escaped += "\\n";
        }
        else
        {
          escaped += c;
        }
      }
      return escaped;
    }

    bool SnapFileWriter::appendRecord(const char *path, const SnapRecord &record)
    {
      if (!path || !*path)
      {
        return false;
      }
      const std::string payload = record.serialize(true);
      FILE *fp = std::fopen(path, "ab");
      if (!fp)
      {
        return false;
      }
      const size_t written = std::fwrite(payload.data(), 1, payload.size(), fp);
      const int flushResult = std::fflush(fp);
      const int closeResult = std::fclose(fp);
      return written == payload.size() && flushResult == 0 && closeResult == 0;
    }
  } // namespace dsl
} // namespace loka
