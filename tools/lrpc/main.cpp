// lrpc - the LRPK package compiler (pack stage).
//
// A host tool, and only a host tool. #185's cross-configure ruling keeps it
// out of the target builds entirely: it is configured natively once into
// `build/host/`, and a cross configure consumes the artifact through
// `LOKA_LRPC`. That is what lets the legacy macOS box, whose gcc-4.0 cannot
// compile modern C++, take part by consuming artifacts only.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "lrpc/LrpkWriter.hpp"
#include "lrpc/PackManifest.hpp"

namespace
{
  using loka::core::resource::lrpk::U32;

  int Fail(const char *message)
  {
    std::fprintf(stderr, "lrpc: %s\n", message);
    return 1;
  }

  int FailAt(const char *message, const std::string &where)
  {
    std::fprintf(stderr, "lrpc: %s: %s\n", message, where.c_str());
    return 1;
  }

  bool ReadWholeFile(const std::string &path, std::vector<unsigned char> &out)
  {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file)
    {
      return false;
    }
    out.clear();
    unsigned char chunk[4096];
    for (;;)
    {
      const std::size_t got = std::fread(chunk, 1, sizeof(chunk), file);
      if (got > 0)
      {
        out.insert(out.end(), chunk, chunk + got);
      }
      if (got < sizeof(chunk))
      {
        break;
      }
    }
    const bool ok = std::ferror(file) == 0;
    std::fclose(file);
    return ok;
  }

  bool WriteWholeFile(const std::string &path, const unsigned char *bytes, std::size_t length)
  {
    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (!file)
    {
      return false;
    }
    const bool written = length == 0 || std::fwrite(bytes, 1, length, file) == length;
    const bool closed = std::fclose(file) == 0;
    return written && closed;
  }

  /** #185 §13: pack always rewrites the whole file, so overwriting in place
      can leave a half-written `.LRP` behind. Write beside the target and
      rename. POSIX renames over an existing file atomically; Windows refuses,
      so the fallback removes first -- losing atomicity only where the platform
      never offered it. */
  bool CommitByRename(const std::string &temporary, const std::string &target)
  {
    if (std::rename(temporary.c_str(), target.c_str()) == 0)
    {
      return true;
    }
    std::remove(target.c_str());
    return std::rename(temporary.c_str(), target.c_str()) == 0;
  }

  /** Collapses a path to a comparable spelling: separators unified, `.` and
      empty components dropped, `..` resolved against what precedes it.

      This is lexical only. It settles `./a` against `a`, `a//b` against `a/b`
      and `d/../a` against `a` -- spellings a build script produces on its own,
      which is what made the previous textual compare inadequate. It does not
      see through symlinks, hard links or a case-insensitive volume; those need
      filesystem identity, which needs the same native path handling as #215
      and is tracked there rather than half-built here. */
  std::string NormalizeForCompare(const std::string &path)
  {
    std::string unified = path;
    for (std::size_t i = 0; i < unified.size(); ++i)
    {
      if (unified[i] == '\\')
      {
        unified[i] = '/';
      }
    }

    const bool rooted = !unified.empty() && unified[0] == '/';
    std::vector<std::string> parts;
    std::string part;
    for (std::size_t i = 0; i <= unified.size(); ++i)
    {
      if (i < unified.size() && unified[i] != '/')
      {
        part.push_back(unified[i]);
        continue;
      }
      if (!part.empty() && part != ".")
      {
        if (part == ".." && !parts.empty() && parts.back() != "..")
        {
          parts.pop_back();
        }
        else
        {
          parts.push_back(part);
        }
      }
      part.clear();
    }

    std::string result = rooted ? "/" : "";
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
      if (i > 0)
      {
        result += "/";
      }
      result += parts[i];
    }
    return result;
  }

  std::string DirectoryOf(const std::string &path)
  {
    const std::string::size_type slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
    {
      return std::string();
    }
    return path.substr(0, slash + 1);
  }

  const char *ManifestMessage(loka::lrpc::ManifestResult result)
  {
    switch (result)
    {
      case loka::lrpc::MANIFEST_UNKNOWN_DIRECTIVE:
        return "unknown directive (expected 'bag' or 'asset')";
      case loka::lrpc::MANIFEST_BAD_FIELD_COUNT:
        return "wrong field count (bag <name> | asset <id> <kind> <name> <source>)";
      case loka::lrpc::MANIFEST_BAD_ID:
        return "asset id is not a decimal 32-bit number";
      case loka::lrpc::MANIFEST_BAD_KIND:
        return "asset kind is not one of image, string, audio";
      case loka::lrpc::MANIFEST_ASSET_BEFORE_BAG:
        return "asset declared before any bag";
      case loka::lrpc::MANIFEST_DUPLICATE_ID:
        return "asset id declared twice";
      case loka::lrpc::MANIFEST_DUPLICATE_BAG:
        return "bag name declared twice";
      case loka::lrpc::MANIFEST_EMPTY:
        return "manifest declares no assets";
      case loka::lrpc::MANIFEST_OK:
        break;
    }
    return "manifest is not valid";
  }

  const char *BuildMessage(loka::lrpc::Writer::BuildResult result)
  {
    switch (result)
    {
      case loka::lrpc::Writer::BUILD_ASSET_WITHOUT_DEFAULT_ROW:
        return "an asset has no axis-free row";
      case loka::lrpc::Writer::BUILD_ASSET_KIND_MISMATCH:
        return "rows for one id disagree on asset kind";
      case loka::lrpc::Writer::BUILD_BAD_BAG_REFERENCE:
        return "a row names a bag that was never declared";
      case loka::lrpc::Writer::BUILD_TOO_MANY_BAGS:
        return "too many bags for the format";
      case loka::lrpc::Writer::BUILD_BAD_ASSET_KIND:
        return "a row carries an asset kind outside the format's set";
      case loka::lrpc::Writer::BUILD_SIZE_OUT_OF_RANGE:
        return "a payload or total size does not fit a 32-bit field";
      case loka::lrpc::Writer::BUILD_NULL_PAYLOAD:
        return "a non-empty asset was supplied without payload bytes";
      default:
        break;
    }
    return "the package could not be built";
  }

  int Usage()
  {
    std::fprintf(stderr,
                 "usage: lrpc pack <manifest> -o <package> [--stamp <file>]\n"
                 "\n"
                 "  Packs canonical, package-ready asset records into an LRPK\n"
                 "  package. No format conversion happens here: payload bytes\n"
                 "  are already in the form the target consumes.\n"
                 "\n"
                 "  --stamp writes the derived id-space stamp as one decimal\n"
                 "  line, for the application build to check its header against.\n");
    return 2;
  }
} // namespace

int main(int argc, char **argv)
{
  if (argc < 2 || std::strcmp(argv[1], "pack") != 0)
  {
    return Usage();
  }

  std::string manifestPath;
  std::string outputPath;
  std::string stampPath;
  for (int i = 2; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "-o")
    {
      if (++i >= argc)
      {
        return Usage();
      }
      outputPath = argv[i];
    }
    else if (arg == "--stamp")
    {
      if (++i >= argc)
      {
        return Usage();
      }
      stampPath = argv[i];
    }
    else if (manifestPath.empty())
    {
      manifestPath = arg;
    }
    else
    {
      return Usage();
    }
  }
  if (manifestPath.empty() || outputPath.empty())
  {
    return Usage();
  }

  std::vector<unsigned char> manifestBytes;
  if (!ReadWholeFile(manifestPath, manifestBytes))
  {
    return FailAt("cannot read manifest", manifestPath);
  }

  loka::lrpc::PackManifest manifest;
  std::size_t errorLine = 0;
  const loka::lrpc::ManifestResult parsed = loka::lrpc::ParseManifest(
      manifestBytes.empty() ? "" : reinterpret_cast<const char *>(&manifestBytes[0]),
      manifestBytes.size(),
      manifest,
      errorLine);
  if (parsed != loka::lrpc::MANIFEST_OK)
  {
    std::fprintf(stderr,
                 "lrpc: %s:%lu: %s\n",
                 manifestPath.c_str(),
                 static_cast<unsigned long>(errorLine),
                 ManifestMessage(parsed));
    return 1;
  }

  // Both outputs are written after the package is committed, so an output that
  // names an input destroys it, and an output that names the other output
  // truncates whichever landed first -- while the command still prints success
  // and exits zero. Refuse before anything is written.
  //
  // The comparison is textual: two spellings of one file (`./a` and `a`, a
  // symlink, a case-insensitive volume) are not detected. That is the cheap
  // half of the check, and it catches the collision a build script actually
  // produces, which is the same variable used twice.
  const std::string base = DirectoryOf(manifestPath);
  {
    std::vector<std::string> inputs;
    inputs.push_back(NormalizeForCompare(manifestPath));
    for (std::size_t i = 0; i < manifest.assets.size(); ++i)
    {
      inputs.push_back(NormalizeForCompare(base + manifest.assets[i].source));
    }
    const std::string outputKey = NormalizeForCompare(outputPath);
    const std::string stampKey =
        stampPath.empty() ? std::string() : NormalizeForCompare(stampPath);
    // The temporary is a third output and can collide too, so it is compared
    // rather than assumed free.
    const std::string temporaryKey = NormalizeForCompare(outputPath + ".tmp");
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
      if (outputKey == inputs[i] || temporaryKey == inputs[i])
      {
        return FailAt("package output would overwrite an input", outputPath);
      }
      if (!stampKey.empty() && stampKey == inputs[i])
      {
        return FailAt("stamp output would overwrite an input", stampPath);
      }
    }
    if (!stampKey.empty() && (stampKey == outputKey || stampKey == temporaryKey))
    {
      return FailAt("stamp and package name the same file", stampPath);
    }
  }

  loka::lrpc::Writer writer;
  for (std::size_t i = 0; i < manifest.bags.size(); ++i)
  {
    writer.addBag();
  }

  // Payloads are held until build() so the writer sees every row before it
  // lays anything out; the alternative would be streaming into a package whose
  // index is not yet decided.
  std::vector<std::vector<unsigned char> > payloads;
  payloads.resize(manifest.assets.size());
  for (std::size_t i = 0; i < manifest.assets.size(); ++i)
  {
    const loka::lrpc::ManifestAsset &asset = manifest.assets[i];
    if (!ReadWholeFile(base + asset.source, payloads[i]))
    {
      return FailAt("cannot read asset payload", base + asset.source);
    }
    writer.addAsset(asset.id,
                    asset.bag,
                    asset.kind,
                    0,
                    payloads[i].empty() ? 0 : &payloads[i][0],
                    payloads[i].size());
  }

  const U32 stamp = loka::lrpc::DeriveIdSpaceStamp(manifest);
  std::vector<unsigned char> package;
  const loka::lrpc::Writer::BuildResult built = writer.build(stamp, package);
  if (built != loka::lrpc::Writer::BUILD_OK)
  {
    return Fail(BuildMessage(built));
  }

  // Both artifacts describe one id space, so neither may replace its previous
  // version until the other is known to be writable. Committing the package
  // first and then failing on an unwritable stamp destination left a new
  // package beside an old or missing stamp -- two artifacts describing
  // different id spaces, which is the state the stamp exists to make
  // impossible. AGENTS.md's failure-atomicity policy states the general form:
  // never destroy the old value before its replacement exists.
  const std::string temporary = outputPath + ".tmp";
  const std::string stampTemporary = stampPath.empty() ? std::string() : stampPath + ".tmp";

  if (!WriteWholeFile(temporary, package.empty() ? 0 : &package[0], package.size()))
  {
    return FailAt("cannot write package", temporary);
  }
  if (!stampPath.empty())
  {
    char line[32];
    const int printed = std::sprintf(line, "%lu\n", static_cast<unsigned long>(stamp));
    if (printed <= 0 ||
        !WriteWholeFile(stampTemporary, reinterpret_cast<const unsigned char *>(line),
                        static_cast<std::size_t>(printed)))
    {
      std::remove(temporary.c_str());
      return FailAt("cannot write stamp", stampTemporary);
    }
  }

  if (!CommitByRename(temporary, outputPath))
  {
    std::remove(temporary.c_str());
    if (!stampTemporary.empty())
    {
      std::remove(stampTemporary.c_str());
    }
    return FailAt("cannot commit package", outputPath);
  }
  if (!stampTemporary.empty() && !CommitByRename(stampTemporary, stampPath))
  {
    // Staging removed every failure that can be seen in advance, so reaching
    // here means the second rename failed on its own. Two files cannot be
    // renamed atomically, so the honest thing is to name both as suspect
    // rather than report a tidy failure.
    std::remove(stampTemporary.c_str());
    std::fprintf(stderr,
                 "lrpc: cannot commit stamp: %s\n"
                 "lrpc: %s is new while the stamp is not -- treat both as out of step\n",
                 stampPath.c_str(),
                 outputPath.c_str());
    return 1;
  }

  std::printf("lrpc: %s (%lu bytes, %lu assets, %lu bags, stamp %lu)\n",
              outputPath.c_str(),
              static_cast<unsigned long>(package.size()),
              static_cast<unsigned long>(manifest.assets.size()),
              static_cast<unsigned long>(manifest.bags.size()),
              static_cast<unsigned long>(stamp));
  return 0;
}
