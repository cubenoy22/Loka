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

#if defined(_WIN32)
#include <direct.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

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

  /** Creates a file that did not exist, and fails if anything is already
      there -- including a dangling symlink, which is the case that made this
      necessary. `fopen("wb")` follows such a link and writes through it, so a
      staging path planted as `pkg.tmp -> pkg` had the tool create `pkg` behind
      its own guard and then rename the link over it, leaving a self-referential
      symlink where the package should be, reported as success.

      Creating exclusively removes the whole class rather than that instance:
      the staging file is one this run made, so it cannot be an alias for
      anything. POSIX only for now -- `fopen` has no portable exclusive mode in
      C++98 -- with the Windows half belonging to #215 along with the rest of
      this tool's native file handling. */
  bool WriteNewFile(const std::string &path, const unsigned char *bytes, std::size_t length)
  {
#if defined(_WIN32)
    return WriteWholeFile(path, bytes, length);
#else
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0)
    {
      return false;
    }
    bool ok = true;
    std::size_t written = 0;
    while (ok && written < length)
    {
      const ssize_t got = write(fd, bytes + written, length - written);
      if (got <= 0)
      {
        ok = false;
        break;
      }
      written += static_cast<std::size_t>(got);
    }
    if (close(fd) != 0)
    {
      ok = false;
    }
    return ok;
#endif
  }

  /** True when the path exists and is a directory. An output that names one is
      refused: `--stamp stamps/` staged into `stamps/.tmp`, and the cleanup path
      then removed the emptied directory the caller had made. */
  bool PathIsDirectory(const std::string &path)
  {
#if defined(_WIN32)
    (void)path;
    return false;
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
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
#if defined(_WIN32)
    // Only where the filesystem agrees. On POSIX a backslash is an ordinary
    // filename character, so folding it here would make `a\b` and `a/b` --
    // two different, legal files -- refuse each other. A false refusal blocks
    // valid work, which is as much a defect as a missed collision.
    for (std::size_t i = 0; i < unified.size(); ++i)
    {
      if (unified[i] == '\\')
      {
        unified[i] = '/';
      }
    }
#endif

    // A drive or share prefix is part of the root, not a component. Treating
    // `C:` as one let `C:/../x` pop it and compare unequal to `C:/x` -- the
    // same file, two keys, and an output free to overwrite an input.
    std::string prefix;
    std::string rest = unified;
    if (rest.size() >= 2 && rest[1] == ':')
    {
      prefix = rest.substr(0, 2);
      rest = rest.substr(2);
    }
    const bool rooted = !rest.empty() && rest[0] == '/';

    std::vector<std::string> parts;
    std::string part;
    for (std::size_t i = 0; i <= rest.size(); ++i)
    {
      if (i < rest.size() && rest[i] != '/')
      {
        part.push_back(rest[i]);
        continue;
      }
      if (!part.empty() && part != ".")
      {
        if (part == ".." && !parts.empty() && parts.back() != "..")
        {
          parts.pop_back();
        }
        else if (part == ".." && (rooted || !prefix.empty()))
        {
          // A root clamps `..`, the way the filesystem does. Keeping the
          // component would leave one file with two keys.
        }
        else
        {
          parts.push_back(part);
        }
      }
      part.clear();
    }

    std::string result = prefix + (rooted ? "/" : "");
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

  /** What the host counts as a path separator, in one place. Normalization,
      rootedness and the manifest's base directory all have to agree; when they
      did not, a manifest named `m\file` on POSIX gave the base `m\`, so a
      source `payload` was read from `m\payload` instead. */
  bool IsSeparator(char c)
  {
#if defined(_WIN32)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
  }

  /** True when the path already names a place, rather than a place relative to
      wherever the process happens to be. Covers the POSIX root, and on Windows
      a drive-qualified path and a UNC share. */
  bool IsRooted(const std::string &path)
  {
    if (path.empty())
    {
      return false;
    }
    if (IsSeparator(path[0]))
    {
      return true;
    }
    return path.size() >= 2 && path[1] == ':';
  }

  /** Resolves a path into one namespace so two spellings of a file compare
      equal. A relative and an absolute spelling normalize to different strings
      otherwise, which let `-o sub/data.bin` overwrite the asset the manifest
      names as `data.bin` beside itself.

      Still lexical. Symlinks, hard links and case-insensitive volumes need
      filesystem identity, tracked with the rest of this tool's native path
      handling in #215. If the working directory cannot be read the key falls
      back to the normalized spelling, which is weaker but never wrong in the
      direction of allowing a collision it would otherwise have caught. */
  std::string AbsoluteKey(const std::string &path)
  {
    if (IsRooted(path))
    {
      return NormalizeForCompare(path);
    }
    char buffer[4096];
#if defined(_WIN32)
    if (_getcwd(buffer, static_cast<int>(sizeof(buffer))) == 0)
#else
    if (getcwd(buffer, sizeof(buffer)) == 0)
#endif
    {
      return NormalizeForCompare(path);
    }
    return NormalizeForCompare(std::string(buffer) + "/" + path);
  }

#if !defined(_WIN32)
  /** Identity of an existing file as the filesystem sees it, so a symlink or a
      hard link cannot present one file under two keys.
    
      Only existing files have one, which is exactly the case that matters: an
      output that does not exist yet cannot already be an input. Returns false
      when the path does not resolve, and the caller falls back to the lexical
      key.
    
      POSIX only. The Windows equivalent is `GetFileInformationByHandle`, which
      needs the same native path handling as the rest of #215 and lands with
      it. Having it on the hosts where the tool runs today is strictly better
      than having it nowhere; the lexical key remains underneath on both. */
  bool IdentityKey(const std::string &path, std::string &out)
  {
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
      return false;
    }
    char buffer[64];
    std::sprintf(buffer,
                 "dev:%lu ino:%lu",
                 static_cast<unsigned long>(info.st_dev),
                 static_cast<unsigned long>(info.st_ino));
    out = buffer;
    return true;
  }
#endif

  /** The key a path is compared under: filesystem identity when the file
      exists and the host can report it, the lexical spelling otherwise. */
  std::string CollisionKey(const std::string &path)
  {
#if !defined(_WIN32)
    std::string identity;
    if (IdentityKey(path, identity))
    {
      return identity;
    }
#endif
    return AbsoluteKey(path);
  }

  std::string DirectoryOf(const std::string &path)
  {
    const std::string::size_type slash =
        path.find_last_of(
#if defined(_WIN32)
            "/\\"
#else
            "/"
#endif
        );
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
      case loka::lrpc::MANIFEST_DUPLICATE_NAME:
        return "symbolic name declared twice";
      case loka::lrpc::MANIFEST_EMBEDDED_NUL:
        return "the manifest contains a NUL byte";
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
  // Every path the run touches, checked as two complete lists rather than as a
  // handful of pairwise comparisons.
  //
  // The pairwise form was wrong three times in a row -- it compared spellings
  // without a common base, and it knew about the package's staging path but
  // not the stamp's. Each miss let an output silently replace an input and
  // still exit zero. The defect was never the individual comparison; it was
  // that "did I remember every path" lived in the reader's head instead of in
  // one place. So the sets are built once and crossed once, and adding an
  // output later means adding it to a list that is already checked against
  // everything.
  const std::string base = DirectoryOf(manifestPath);
  {
    std::vector<std::string> inputs;
    std::vector<std::string> inputNames;
    inputs.push_back(CollisionKey(manifestPath));
    inputNames.push_back(manifestPath);
    for (std::size_t i = 0; i < manifest.assets.size(); ++i)
    {
      inputs.push_back(CollisionKey(base + manifest.assets[i].source));
      inputNames.push_back(base + manifest.assets[i].source);
    }

    std::vector<std::string> outputs;
    std::vector<std::string> outputPaths;
    std::vector<std::string> outputNames;
    // Named by role as well as by path: two outputs that collide often share
    // a spelling, and "Z.tmp and Z.tmp name the same file" tells the author
    // nothing about which two things met there.
    outputPaths.push_back(outputPath);
    outputNames.push_back("the package (" + outputPath + ")");
    outputPaths.push_back(outputPath + ".tmp");
    outputNames.push_back("the package staging file (" + outputPath + ".tmp)");
    if (!stampPath.empty())
    {
      outputPaths.push_back(stampPath);
      outputNames.push_back("the stamp (" + stampPath + ")");
      outputPaths.push_back(stampPath + ".tmp");
      outputNames.push_back("the stamp staging file (" + stampPath + ".tmp)");
    }
    for (std::size_t i = 0; i < outputPaths.size(); ++i)
    {
      outputs.push_back(CollisionKey(outputPaths[i]));
    }

    for (std::size_t o = 0; o < outputs.size(); ++o)
    {
      // A directory is not a file this run can replace, and treating it as one
      // is destructive: `--stamp stamps/` staged into `stamps/.tmp`, and the
      // cleanup path then removed the emptied directory the caller had made.
      if (PathIsDirectory(outputPaths[o]))
      {
        std::fprintf(stderr, "lrpc: %s is a directory\n", outputNames[o].c_str());
        return 1;
      }
      for (std::size_t i = 0; i < inputs.size(); ++i)
      {
        if (outputs[o] == inputs[i])
        {
          std::fprintf(stderr,
                       "lrpc: %s would overwrite the input %s\n",
                       outputNames[o].c_str(),
                       inputNames[i].c_str());
          return 1;
        }
      }
      for (std::size_t p = o + 1; p < outputs.size(); ++p)
      {
        if (outputs[o] == outputs[p])
        {
          std::fprintf(stderr,
                       "lrpc: %s and %s name the same file\n",
                       outputNames[o].c_str(),
                       outputNames[p].c_str());
          return 1;
        }
      }
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

  if (!WriteNewFile(temporary, package.empty() ? 0 : &package[0], package.size()))
  {
    return FailAt("cannot create package staging file", temporary);
  }
  if (!stampPath.empty())
  {
    char line[32];
    const int printed = std::sprintf(line, "%lu\n", static_cast<unsigned long>(stamp));
    if (printed <= 0 ||
        !WriteNewFile(stampTemporary, reinterpret_cast<const unsigned char *>(line),
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
    // renamed atomically, so the remaining choice is which inconsistent state
    // to leave behind -- and a stale stamp beside a new package is the worse
    // one, because it agrees with a package that no longer exists and the
    // build reads it as truth. Removing it leaves the stamp absent instead,
    // which the consuming build cannot mistake for an answer. The package
    // still carries its own stamp in HEAD, so nothing is lost that cannot be
    // read back out of the artifact itself.
    std::remove(stampTemporary.c_str());
    const bool staleRemoved = !PathIsDirectory(stampPath) && std::remove(stampPath.c_str()) == 0;
    std::fprintf(stderr,
                 "lrpc: cannot commit stamp: %s\n"
                 "lrpc: %s was written; the stamp file is %s -- read the stamp "
                 "from the package rather than trusting a stale one\n",
                 stampPath.c_str(),
                 outputPath.c_str(),
                 staleRemoved ? "removed" : "possibly stale and could not be removed");
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
