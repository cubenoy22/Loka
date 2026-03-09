#include "loka/dsl/SnapFormat.hpp"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
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

      static std::string trim(const std::string &value)
      {
        size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
        {
          ++begin;
        }
        size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
          --end;
        }
        return value.substr(begin, end - begin);
      }

      static bool isAbsolutePath(const std::string &path)
      {
        if (path.empty())
        {
          return false;
        }
        if (path[0] == '/' || path[0] == '\\')
        {
          return true;
        }
        if (path.size() >= 3 &&
            ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
            path[1] == ':' &&
            (path[2] == '\\' || path[2] == '/'))
        {
          return true;
        }
        return false;
      }

      static bool parseLong(const std::string &value, long &out)
      {
        if (value.empty())
        {
          return false;
        }
        char *end = 0;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (!end)
        {
          return false;
        }
        if (*end != '\0')
        {
          return false;
        }
        out = parsed;
        return true;
      }

      static bool readSettings(const char *configPath, SnapTestConfig::Settings &out)
      {
        if (!configPath || !*configPath)
        {
          return false;
        }

        FILE *fp = std::fopen(configPath, "rb");
        if (!fp)
        {
          return false;
        }

        char line[1024];
        while (std::fgets(line, sizeof(line), fp) != 0)
        {
          std::string raw(line);
          std::string entry = trim(raw);
          if (entry.empty())
          {
            continue;
          }
          if (entry[0] == '#')
          {
            continue;
          }

          const size_t eq = entry.find('=');
          if (eq == std::string::npos)
          {
            continue;
          }

          const std::string key = trim(entry.substr(0, eq));
          const std::string value = trim(entry.substr(eq + 1));

          if (key == "capture_dir")
          {
            if (!value.empty())
            {
              out.captureDir = value;
              out.hasCaptureDir = true;
            }
            continue;
          }

          if (key == "max_files")
          {
            long parsed = 0;
            if (parseLong(value, parsed))
            {
              out.maxFiles = parsed;
              out.hasMaxFiles = true;
            }
            continue;
          }

          if (key == "max_total_bytes")
          {
            long parsed = 0;
            if (parseLong(value, parsed))
            {
              out.maxTotalBytes = parsed;
              out.hasMaxTotalBytes = true;
            }
            continue;
          }
        }

        std::fclose(fp);
        return true;
      }

      static std::string joinPath(const std::string &base, const std::string &leaf)
      {
        if (base.empty())
        {
          return leaf;
        }
        const char last = base[base.size() - 1];
        if (last == '/' || last == '\\')
        {
          return base + leaf;
        }
        return base + "/" + leaf;
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

    std::string SnapTestConfig::resolveCapturePath(const char *path, const char *configPath)
    {
      if (!path)
      {
        return "";
      }

      const std::string pathStr(path);
      if (pathStr.empty() || isAbsolutePath(pathStr))
      {
        return pathStr;
      }

      SnapTestConfig::Settings settings;
      if (!readSettings(configPath, settings) || !settings.hasCaptureDir || settings.captureDir.empty())
      {
        return pathStr;
      }

      return joinPath(settings.captureDir, pathStr);
    }

    bool SnapTestConfig::load(const char *configPath, Settings &out)
    {
      out = Settings();
      return readSettings(configPath, out);
    }
  } // namespace dsl
} // namespace loka
