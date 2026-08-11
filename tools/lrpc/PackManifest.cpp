#include "lrpc/PackManifest.hpp"

#include <algorithm>
#include <sstream>

#include "lrpc/HostFile.hpp"

namespace loka
{
  namespace lrpc
  {
    namespace
    {
      using core::resource::lrpk::AssetKind;
      using core::resource::lrpk::U32;

      bool IsSpace(char c)
      {
        return c == ' ' || c == '\t' || c == '\r';
      }

      /** Splits one line into whitespace-separated fields, dropping a `#`
          comment. Fields are returned by value: manifests are host-side build
          inputs measured in kilobytes, so the copy costs nothing and the
          alternative (pointers into the caller's buffer) would tie the parsed
          result's lifetime to the text. */
      void SplitFields(const std::string &line, std::vector<std::string> &out)
      {
        out.clear();
        std::string field;
        for (std::size_t i = 0; i < line.size(); ++i)
        {
          const char c = line[i];
          if (c == '#')
          {
            break;
          }
          if (IsSpace(c))
          {
            if (!field.empty())
            {
              out.push_back(field);
              field.clear();
            }
            continue;
          }
          field.push_back(c);
        }
        if (!field.empty())
        {
          out.push_back(field);
        }
      }

      /** Decimal only, and the whole field must be consumed. Accepting hex or
          a trailing suffix here would let a typo become a different id in a
          package that built cleanly. */
      bool ParseU32(const std::string &text, U32 &out)
      {
        if (text.empty() || text.size() > 10)
        {
          return false;
        }
        U32 value = 0;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
          const char c = text[i];
          if (c < '0' || c > '9')
          {
            return false;
          }
          const U32 digit = static_cast<U32>(c - '0');
          if (value > (0xFFFFFFFFu - digit) / 10u)
          {
            return false;
          }
          value = value * 10u + digit;
        }
        out = value;
        return true;
      }

      bool ParseKind(const std::string &text, AssetKind &out)
      {
        if (text == "image")
        {
          out = core::resource::lrpk::ASSET_KIND_IMAGE;
          return true;
        }
        if (text == "string")
        {
          out = core::resource::lrpk::ASSET_KIND_STRING;
          return true;
        }
        if (text == "audio")
        {
          out = core::resource::lrpk::ASSET_KIND_AUDIO;
          return true;
        }
        // ASSET_KIND_UNKNOWN is deliberately not spellable: a row carrying it
        // is what the writer's BUILD_BAD_ASSET_KIND wall exists to refuse.
        return false;
      }

      const char *KindName(AssetKind kind)
      {
        switch (kind)
        {
          case core::resource::lrpk::ASSET_KIND_UNKNOWN:
            return "unknown";
          case core::resource::lrpk::ASSET_KIND_IMAGE:
            return "image";
          case core::resource::lrpk::ASSET_KIND_STRING:
            return "string";
          case core::resource::lrpk::ASSET_KIND_AUDIO:
            return "audio";
        }
        return "unknown";
      }

      const ManifestAsset *FindAsset(const PackManifest &manifest, U32 id)
      {
        for (std::size_t i = 0; i < manifest.assets.size(); ++i)
        {
          if (manifest.assets[i].id == id)
          {
            return &manifest.assets[i];
          }
        }
        return 0;
      }

      void AddViolation(std::size_t line,
                        const std::string &message,
                        std::vector<RequirementViolation> &violations)
      {
        RequirementViolation violation;
        violation.line = line;
        violation.message = message;
        violations.push_back(violation);
      }

      void CheckBagRequirement(std::size_t line,
                               std::size_t index,
                               const std::string &name,
                               const PackManifest &manifest,
                               std::vector<RequirementViolation> &violations)
      {
        if (index >= manifest.bags.size())
        {
          std::ostringstream message;
          message << "bag " << index << " must be named \"" << name
                  << "\"; found only " << manifest.bags.size() << " bags";
          AddViolation(line, message.str(), violations);
        }
        else if (manifest.bags[index] != name)
        {
          std::ostringstream message;
          message << "bag " << index << " must be named \"" << name
                  << "\"; found bag " << index << " named \""
                  << manifest.bags[index] << "\"";
          AddViolation(line, message.str(), violations);
        }
      }

      void CheckAssetRequirement(std::size_t line,
                                 U32 id,
                                 AssetKind kind,
                                 const PackManifest &manifest,
                                 std::vector<RequirementViolation> &violations)
      {
        const ManifestAsset *asset = FindAsset(manifest, id);
        if (!asset)
        {
          std::ostringstream message;
          message << "asset " << id << " must have kind " << KindName(kind)
                  << "; found no asset with that id";
          AddViolation(line, message.str(), violations);
        }
        else if (asset->kind != kind)
        {
          std::ostringstream message;
          message << "asset " << id << " must have kind " << KindName(kind)
                  << "; found kind " << KindName(asset->kind);
          AddViolation(line, message.str(), violations);
        }
      }

      void CheckPagesRequirement(std::size_t line,
                                 U32 firstId,
                                 std::size_t firstBag,
                                 const PackManifest &manifest,
                                 std::vector<RequirementViolation> &violations)
      {
        std::size_t pageCount = 0;
        while (pageCount < manifest.assets.size() &&
               pageCount <= static_cast<std::size_t>(0xFFFFFFFFu - firstId) &&
               FindAsset(manifest, firstId + static_cast<U32>(pageCount)))
        {
          ++pageCount;
        }
        const std::size_t bagCount = firstBag < manifest.bags.size()
                                         ? manifest.bags.size() - firstBag
                                         : 0;

        std::ostringstream message;
        message << "pages from asset " << firstId
                << " must occupy bags from " << firstBag
                << " one per bag; found ";
        if (pageCount == 0)
        {
          message << "no asset " << firstId;
          AddViolation(line, message.str(), violations);
          return;
        }
        if (pageCount != bagCount)
        {
          message << pageCount << " consecutive page assets but " << bagCount
                  << " bags from index " << firstBag;
          AddViolation(line, message.str(), violations);
          return;
        }

        for (std::size_t page = 0; page < pageCount; ++page)
        {
          const U32 id = firstId + static_cast<U32>(page);
          const ManifestAsset *asset = FindAsset(manifest, id);
          const std::size_t expectedBag = firstBag + page;
          if (asset->bag != expectedBag)
          {
            message << "asset " << id << " in bag " << asset->bag
                    << " (\"" << manifest.bags[asset->bag]
                    << "\"), expected bag " << expectedBag << " (\""
                    << manifest.bags[expectedBag] << "\")";
            AddViolation(line, message.str(), violations);
            return;
          }
        }
      }

      struct StampRow
      {
        StampRow()
            : id(0),
              kind(0),
              name()
        {
        }

        U32 id;
        unsigned char kind;
        std::string name;

        bool operator<(const StampRow &other) const
        {
          if (id != other.id)
          {
            return id < other.id;
          }
          if (kind != other.kind)
          {
            return kind < other.kind;
          }
          return name < other.name;
        }
      };
    } // namespace

    ManifestResult ParseManifest(const char *text,
                                 std::size_t length,
                                 PackManifest &out,
                                 std::size_t &errorLine)
    {
      errorLine = 0;
      PackManifest parsed;

      // Refused for the whole file rather than per field: the danger is that a
      // NUL survives into a path, and the cheapest place to be sure it cannot
      // is before anything is split.
      for (std::size_t i = 0; i < length; ++i)
      {
        if (text[i] == '\0')
        {
          return MANIFEST_EMBEDDED_NUL;
        }
      }

      std::string line;
      std::vector<std::string> fields;
      std::size_t lineNumber = 0;
      std::size_t cursor = 0;

      while (cursor <= length)
      {
        const bool atEnd = cursor == length;
        const char c = atEnd ? '\n' : text[cursor];
        ++cursor;
        if (c != '\n')
        {
          line.push_back(c);
          continue;
        }
        ++lineNumber;
        SplitFields(line, fields);
        line.clear();
        if (fields.empty())
        {
          if (atEnd)
          {
            break;
          }
          continue;
        }

        if (fields[0] == "bag")
        {
          if (fields.size() != 2)
          {
            errorLine = lineNumber;
            return MANIFEST_BAD_FIELD_COUNT;
          }
          for (std::size_t i = 0; i < parsed.bags.size(); ++i)
          {
            if (parsed.bags[i] == fields[1])
            {
              errorLine = lineNumber;
              return MANIFEST_DUPLICATE_BAG;
            }
          }
          parsed.bags.push_back(fields[1]);
        }
        else if (fields[0] == "asset")
        {
          if (fields.size() != 5)
          {
            errorLine = lineNumber;
            return MANIFEST_BAD_FIELD_COUNT;
          }
          if (parsed.bags.empty())
          {
            errorLine = lineNumber;
            return MANIFEST_ASSET_BEFORE_BAG;
          }
          ManifestAsset asset;
          if (!ParseU32(fields[1], asset.id))
          {
            errorLine = lineNumber;
            return MANIFEST_BAD_ID;
          }
          if (!ParseKind(fields[2], asset.kind))
          {
            errorLine = lineNumber;
            return MANIFEST_BAD_KIND;
          }
          // One row per id in the slice: representations arrive with the axis
          // work, and until then a repeated id is a mistake rather than a
          // second representation.
          for (std::size_t i = 0; i < parsed.assets.size(); ++i)
          {
            if (parsed.assets[i].id == asset.id)
            {
              errorLine = lineNumber;
              return MANIFEST_DUPLICATE_ID;
            }
            if (parsed.assets[i].name == fields[3])
            {
              errorLine = lineNumber;
              return MANIFEST_DUPLICATE_NAME;
            }
          }
          asset.bag = parsed.bags.size() - 1;
          asset.name = fields[3];
          asset.source = fields[4];
          parsed.assets.push_back(asset);
        }
        else
        {
          errorLine = lineNumber;
          return MANIFEST_UNKNOWN_DIRECTIVE;
        }

        if (atEnd)
        {
          break;
        }
      }

      if (parsed.assets.empty())
      {
        return MANIFEST_EMPTY;
      }

      out = parsed;
      return MANIFEST_OK;
    }

    RequirementResult CheckPackageRequirements(
        const char *text,
        std::size_t length,
        const PackManifest &manifest,
        std::vector<RequirementViolation> &violations,
        std::size_t &errorLine)
    {
      errorLine = 0;
      violations.clear();
      std::size_t requirementCount = 0;

      for (std::size_t i = 0; i < length; ++i)
      {
        if (text[i] == '\0')
        {
          return REQUIREMENTS_EMBEDDED_NUL;
        }
      }

      std::string line;
      std::vector<std::string> fields;
      std::size_t lineNumber = 0;
      std::size_t cursor = 0;
      while (cursor <= length)
      {
        const bool atEnd = cursor == length;
        const char c = atEnd ? '\n' : text[cursor];
        ++cursor;
        if (c != '\n')
        {
          line.push_back(c);
          continue;
        }
        ++lineNumber;
        SplitFields(line, fields);
        line.clear();
        if (fields.empty())
        {
          if (atEnd)
          {
            break;
          }
          continue;
        }

        errorLine = lineNumber;
        ++requirementCount;
        if (fields[0] == "bag")
        {
          if (fields.size() != 3)
          {
            return REQUIREMENTS_BAD_FIELD_COUNT;
          }
          U32 index = 0;
          if (!ParseU32(fields[1], index))
          {
            return REQUIREMENTS_BAD_INDEX;
          }
          CheckBagRequirement(lineNumber,
                              static_cast<std::size_t>(index),
                              fields[2],
                              manifest,
                              violations);
        }
        else if (fields[0] == "asset")
        {
          if (fields.size() != 3)
          {
            return REQUIREMENTS_BAD_FIELD_COUNT;
          }
          U32 id = 0;
          if (!ParseU32(fields[1], id))
          {
            return REQUIREMENTS_BAD_ID;
          }
          AssetKind kind = core::resource::lrpk::ASSET_KIND_UNKNOWN;
          if (!ParseKind(fields[2], kind))
          {
            return REQUIREMENTS_BAD_KIND;
          }
          CheckAssetRequirement(lineNumber, id, kind, manifest, violations);
        }
        else if (fields[0] == "pages")
        {
          if (fields.size() != 5)
          {
            return REQUIREMENTS_BAD_FIELD_COUNT;
          }
          if (fields[2] != "count-from" || fields[3] != "bag")
          {
            return REQUIREMENTS_BAD_PAGES_FORM;
          }
          U32 firstId = 0;
          if (!ParseU32(fields[1], firstId))
          {
            return REQUIREMENTS_BAD_ID;
          }
          U32 firstBag = 0;
          if (!ParseU32(fields[4], firstBag))
          {
            return REQUIREMENTS_BAD_INDEX;
          }
          CheckPagesRequirement(lineNumber,
                                firstId,
                                static_cast<std::size_t>(firstBag),
                                manifest,
                                violations);
        }
        else
        {
          return REQUIREMENTS_UNKNOWN_DIRECTIVE;
        }

        if (atEnd)
        {
          break;
        }
      }

      if (requirementCount == 0)
      {
        errorLine = 0;
        return REQUIREMENTS_EMPTY;
      }

      errorLine = 0;
      return REQUIREMENTS_OK;
    }

    RequirementResult CheckPackageRequirementsFile(
        const char *path,
        const PackManifest &manifest,
        std::vector<RequirementViolation> &violations,
        std::size_t &errorLine)
    {
      errorLine = 0;
      std::vector<unsigned char> bytes;
      if (!ReadWholeFile(std::string(path), bytes))
      {
        return REQUIREMENTS_CANNOT_READ;
      }
      return CheckPackageRequirements(
          bytes.empty() ? "" : reinterpret_cast<const char *>(&bytes[0]),
          bytes.size(),
          manifest,
          violations,
          errorLine);
    }

#if defined(_WIN32)
    RequirementResult CheckPackageRequirementsFile(
        const wchar_t *path,
        const PackManifest &manifest,
        std::vector<RequirementViolation> &violations,
        std::size_t &errorLine)
    {
      errorLine = 0;
      std::vector<unsigned char> bytes;
      if (!ReadWholeFile(std::wstring(path), bytes))
      {
        return REQUIREMENTS_CANNOT_READ;
      }
      return CheckPackageRequirements(
          bytes.empty() ? "" : reinterpret_cast<const char *>(&bytes[0]),
          bytes.size(),
          manifest,
          violations,
          errorLine);
    }
#endif

    core::resource::lrpk::U32 DeriveIdSpaceStamp(const PackManifest &manifest)
    {
      std::vector<StampRow> rows;
      rows.reserve(manifest.assets.size());
      for (std::size_t i = 0; i < manifest.assets.size(); ++i)
      {
        StampRow row;
        row.id = manifest.assets[i].id;
        row.kind = static_cast<unsigned char>(manifest.assets[i].kind);
        row.name = manifest.assets[i].name;
        rows.push_back(row);
      }
      // Sorted so the stamp describes the id space itself rather than the
      // order the manifest happened to list it in. Reordering the manifest
      // must not restamp a package whose ids did not move.
      std::sort(rows.begin(), rows.end());

      core::resource::lrpk::Crc32 crc;
      for (std::size_t i = 0; i < rows.size(); ++i)
      {
        // The name is part of the association, not decoration. Hashing only
        // `(id, kind)` leaves two same-kind symbols free to exchange ids
        // without moving the stamp: the row multiset is unchanged while the
        // generated header now points each symbol at the other's bytes, so an
        // old package passes the check and draws the wrong asset. That is the
        // silent mismatch the stamp exists to catch.
        unsigned char encoded[9];
        core::resource::lrpk::WriteU32BE(encoded, rows[i].id);
        encoded[4] = rows[i].kind;
        // Length-prefixed so that adjacent names cannot be re-cut into the
        // same byte stream ("AB" + "C" must not hash as "A" + "BC").
        core::resource::lrpk::WriteU32BE(encoded + 5,
                                         static_cast<U32>(rows[i].name.size()));
        crc.update(encoded, sizeof(encoded));
        if (!rows[i].name.empty())
        {
          crc.update(reinterpret_cast<const unsigned char *>(rows[i].name.data()),
                     rows[i].name.size());
        }
      }
      return crc.value();
    }
  } // namespace lrpc
} // namespace loka
