// lrpc - the LRPK package compiler (pack stage).
//
// A host tool, and only a host tool. #185's cross-configure ruling keeps it
// out of the target builds entirely: it is configured natively once into
// `build/host/`, and a cross configure consumes the artifact through
// `LOKA_LRPC`. That is what lets the legacy macOS box, whose gcc-4.0 cannot
// compile modern C++, take part by consuming artifacts only.

#include <cstdio>
#include <cwchar>
#include <string>
#include <sys/stat.h>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "lrpc/LrpkWriter.hpp"
#include "lrpc/HostFile.hpp"
#include "lrpc/PackManifest.hpp"
#include "lrpc/Utf8Path.hpp"

namespace
{
  using loka::core::resource::lrpk::U32;

#if defined(_WIN32)
  typedef wchar_t NativeChar;
  typedef std::wstring NativePath;
#define LRPC_NATIVE_TEXT(value) L##value
#else
  typedef char NativeChar;
  typedef std::string NativePath;
#define LRPC_NATIVE_TEXT(value) value
#endif

  int Fail(const char *message)
  {
    std::fprintf(stderr, "lrpc: %s\n", message);
    return 1;
  }

  int FailAt(const NativeChar *message, const NativePath &where)
  {
#if defined(_WIN32)
    std::fwprintf(stderr, L"lrpc: %ls: %ls\n", message, where.c_str());
#else
    std::fprintf(stderr, "lrpc: %s: %s\n", message, where.c_str());
#endif
    return 1;
  }

  bool ParseNativeU32(const NativePath &text, std::size_t &out)
  {
    if (text.empty() || text.size() > 10)
    {
      return false;
    }
    U32 value = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
      const NativeChar c = text[i];
      if (c < LRPC_NATIVE_TEXT('0') || c > LRPC_NATIVE_TEXT('9'))
      {
        return false;
      }
      const U32 digit = static_cast<U32>(c - LRPC_NATIVE_TEXT('0'));
      if (value > (0xFFFFFFFFu - digit) / 10u)
      {
        return false;
      }
      value = value * 10u + digit;
    }
    out = static_cast<std::size_t>(value);
    return true;
  }

  std::FILE *OpenFile(const NativePath &path, const NativeChar *mode)
  {
#if defined(_WIN32)
    return _wfopen(path.c_str(), mode);
#else
    return std::fopen(path.c_str(), mode);
#endif
  }

  int RemoveFile(const NativePath &path)
  {
#if defined(_WIN32)
    return _wremove(path.c_str());
#else
    return std::remove(path.c_str());
#endif
  }

  int RenameFile(const NativePath &from, const NativePath &to)
  {
#if defined(_WIN32)
    return _wrename(from.c_str(), to.c_str());
#else
    return std::rename(from.c_str(), to.c_str());
#endif
  }

  bool WriteWholeFile(const NativePath &path,
                      const unsigned char *bytes,
                      std::size_t length)
  {
    std::FILE *file = OpenFile(path, LRPC_NATIVE_TEXT("wb"));
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
      anything. POSIX keeps the exclusive create; the Windows wide CRT has no
      equivalent mode in the compiler baselines this host tool supports. */
  bool WriteNewFile(const NativePath &path,
                    const unsigned char *bytes,
                    std::size_t length)
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
  bool PathIsDirectory(const NativePath &path)
  {
#if defined(_WIN32)
    struct _stat info;
    return _wstat(path.c_str(), &info) == 0 && (info.st_mode & _S_IFDIR) != 0;
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
  bool CommitByRename(const NativePath &temporary, const NativePath &target)
  {
    if (RenameFile(temporary, target) == 0)
    {
      return true;
    }
    RemoveFile(target);
    return RenameFile(temporary, target) == 0;
  }

  /** Collapses a path to a comparable spelling: separators unified, `.` and
      empty components dropped, `..` resolved against what precedes it.

      This is lexical only. It settles `./a` against `a`, `a//b` against `a/b`
      and `d/../a` against `a` -- spellings a build script produces on its own,
      which is what made the previous textual compare inadequate. POSIX adds
      filesystem identity below; Windows keeps the wide lexical key. */
  NativePath NormalizeForCompare(const NativePath &path)
  {
    NativePath unified = path;
#if defined(_WIN32)
    // Only where the filesystem agrees. On POSIX a backslash is an ordinary
    // filename character, so folding it here would make `a\b` and `a/b` --
    // two different, legal files -- refuse each other. A false refusal blocks
    // valid work, which is as much a defect as a missed collision.
    for (std::size_t i = 0; i < unified.size(); ++i)
    {
      if (unified[i] == L'\\')
      {
        unified[i] = L'/';
      }
    }
#endif

    // A drive or share prefix is part of the root, not a component. Treating
    // `C:` as one let `C:/../x` pop it and compare unequal to `C:/x` -- the
    // same file, two keys, and an output free to overwrite an input.
    NativePath prefix;
    NativePath rest = unified;
    if (rest.size() >= 2 && rest[1] == LRPC_NATIVE_TEXT(':'))
    {
      prefix = rest.substr(0, 2);
      rest = rest.substr(2);
    }
    const bool rooted = !rest.empty() && rest[0] == LRPC_NATIVE_TEXT('/');

    std::vector<NativePath> parts;
    NativePath part;
    for (std::size_t i = 0; i <= rest.size(); ++i)
    {
      if (i < rest.size() && rest[i] != LRPC_NATIVE_TEXT('/'))
      {
        part.push_back(rest[i]);
        continue;
      }
      if (!part.empty() && part != LRPC_NATIVE_TEXT("."))
      {
        if (part == LRPC_NATIVE_TEXT("..") && !parts.empty() &&
            parts.back() != LRPC_NATIVE_TEXT(".."))
        {
          parts.pop_back();
        }
        else if (part == LRPC_NATIVE_TEXT("..") && (rooted || !prefix.empty()))
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

    NativePath result = prefix +
                        (rooted ? LRPC_NATIVE_TEXT("/") : LRPC_NATIVE_TEXT(""));
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
      if (i > 0)
      {
        result += LRPC_NATIVE_TEXT("/");
      }
      result += parts[i];
    }
    return result;
  }

  /** What the host counts as a path separator, in one place. Normalization,
      rootedness and the manifest's base directory all have to agree; when they
      did not, a manifest named `m\file` on POSIX gave the base `m\`, so a
      source `payload` was read from `m\payload` instead. */
  bool IsSeparator(NativeChar c)
  {
#if defined(_WIN32)
    return c == L'/' || c == L'\\';
#else
    return c == '/';
#endif
  }

  /** True when the path already names a place, rather than a place relative to
      wherever the process happens to be. Covers the POSIX root, and on Windows
      a drive-qualified path and a UNC share. */
  bool IsRooted(const NativePath &path)
  {
    if (path.empty())
    {
      return false;
    }
    if (IsSeparator(path[0]))
    {
      return true;
    }
    return path.size() >= 2 && path[1] == LRPC_NATIVE_TEXT(':');
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
  NativePath AbsoluteKey(const NativePath &path)
  {
    if (IsRooted(path))
    {
      return NormalizeForCompare(path);
    }
    NativeChar buffer[4096];
#if defined(_WIN32)
    if (_wgetcwd(buffer,
                 static_cast<int>(sizeof(buffer) / sizeof(buffer[0]))) == 0)
#else
    if (getcwd(buffer, sizeof(buffer)) == 0)
#endif
    {
      return NormalizeForCompare(path);
    }
    return NormalizeForCompare(NativePath(buffer) + LRPC_NATIVE_TEXT("/") + path);
  }

#if !defined(_WIN32)
  /** Identity of an existing file as the filesystem sees it, so a symlink or a
      hard link cannot present one file under two keys.
    
      Only existing files have one, which is exactly the case that matters: an
      output that does not exist yet cannot already be an input. Returns false
      when the path does not resolve, and the caller falls back to the lexical
      key.
    
      POSIX only. Windows still uses the native wide lexical key underneath;
      filesystem-identity collision hardening is independent of opening every
      user path without loss. */
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
  NativePath CollisionKey(const NativePath &path)
  {
#if !defined(_WIN32)
    NativePath identity;
    if (IdentityKey(path, identity))
    {
      return identity;
    }
#endif
    return AbsoluteKey(path);
  }

  NativePath DirectoryOf(const NativePath &path)
  {
    const NativePath::size_type slash =
        path.find_last_of(
#if defined(_WIN32)
            L"/\\"
#else
            "/"
#endif
        );
    if (slash == NativePath::npos)
    {
      return NativePath();
    }
    return path.substr(0, slash + 1);
  }

  const NativeChar *ManifestMessage(loka::lrpc::ManifestResult result)
  {
    switch (result)
    {
      case loka::lrpc::MANIFEST_UNKNOWN_DIRECTIVE:
        return LRPC_NATIVE_TEXT("unknown directive (expected 'bag' or 'asset')");
      case loka::lrpc::MANIFEST_BAD_FIELD_COUNT:
        return LRPC_NATIVE_TEXT("wrong field count (bag <name> | asset <id> <kind> <name> <source>)");
      case loka::lrpc::MANIFEST_BAD_ID:
        return LRPC_NATIVE_TEXT("asset id is not a decimal 32-bit number");
      case loka::lrpc::MANIFEST_BAD_KIND:
        return LRPC_NATIVE_TEXT("asset kind is not one of image, string, audio");
      case loka::lrpc::MANIFEST_ASSET_BEFORE_BAG:
        return LRPC_NATIVE_TEXT("asset declared before any bag");
      case loka::lrpc::MANIFEST_DUPLICATE_ID:
        return LRPC_NATIVE_TEXT("asset id declared twice");
      case loka::lrpc::MANIFEST_DUPLICATE_NAME:
        return LRPC_NATIVE_TEXT("symbolic name declared twice");
      case loka::lrpc::MANIFEST_EMBEDDED_NUL:
        return LRPC_NATIVE_TEXT("the manifest contains a NUL byte");
      case loka::lrpc::MANIFEST_DUPLICATE_BAG:
        return LRPC_NATIVE_TEXT("bag name declared twice");
      case loka::lrpc::MANIFEST_EMPTY:
        return LRPC_NATIVE_TEXT("manifest declares no assets");
      case loka::lrpc::MANIFEST_OK:
        break;
    }
    return LRPC_NATIVE_TEXT("manifest is not valid");
  }

  const NativeChar *RequirementMessage(loka::lrpc::RequirementResult result)
  {
    switch (result)
    {
      case loka::lrpc::REQUIREMENTS_UNKNOWN_DIRECTIVE:
        return LRPC_NATIVE_TEXT("unknown requirement (expected 'bag', 'asset', or 'pages')");
      case loka::lrpc::REQUIREMENTS_BAD_FIELD_COUNT:
        return LRPC_NATIVE_TEXT("wrong field count (bag <index> <name> | asset <id> <kind> in <bag-name> | pages <id> count-from bag <index> kinds <kind-list>)");
      case loka::lrpc::REQUIREMENTS_BAD_INDEX:
        return LRPC_NATIVE_TEXT("bag index is not a decimal 32-bit number");
      case loka::lrpc::REQUIREMENTS_BAD_ID:
        return LRPC_NATIVE_TEXT("asset id is not a decimal 32-bit number");
      case loka::lrpc::REQUIREMENTS_BAD_KIND:
        return LRPC_NATIVE_TEXT("asset kind is not one of image, string, audio");
      case loka::lrpc::REQUIREMENTS_BAD_ASSET_FORM:
        return LRPC_NATIVE_TEXT("asset requirement must say 'in <bag-name>'");
      case loka::lrpc::REQUIREMENTS_BAD_PAGES_FORM:
        return LRPC_NATIVE_TEXT("pages requirement must say 'count-from bag <index> kinds <kind-list>'");
      case loka::lrpc::REQUIREMENTS_PAGE_COUNT_REQUIRED:
        return LRPC_NATIVE_TEXT("pages requirement needs --require-pages <N>");
      case loka::lrpc::REQUIREMENTS_EMBEDDED_NUL:
        return LRPC_NATIVE_TEXT("the requirements file contains a NUL byte");
      case loka::lrpc::REQUIREMENTS_EMPTY:
        return LRPC_NATIVE_TEXT("requirements file declares no requirements");
      case loka::lrpc::REQUIREMENTS_CANNOT_READ:
        return LRPC_NATIVE_TEXT("cannot read requirements");
      case loka::lrpc::REQUIREMENTS_OK:
        break;
    }
    return LRPC_NATIVE_TEXT("requirements file is not valid");
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

  bool ResolveAssetPath(const NativePath &base,
                        const std::string &utf8Source,
                        NativePath &out)
  {
#if defined(_WIN32)
    std::wstring nativeSource;
    if (!loka::lrpc::Utf8PathToWide(utf8Source, nativeSource))
    {
      return false;
    }
    out = base + nativeSource;
#else
    out = base + utf8Source;
#endif
    return true;
  }

  int FailManifest(const NativePath &path,
                   std::size_t line,
                   loka::lrpc::ManifestResult result)
  {
#if defined(_WIN32)
    std::fwprintf(stderr,
                  L"lrpc: %ls:%lu: %ls\n",
                  path.c_str(),
                  static_cast<unsigned long>(line),
                  ManifestMessage(result));
#else
    std::fprintf(stderr,
                 "lrpc: %s:%lu: %s\n",
                 path.c_str(),
                 static_cast<unsigned long>(line),
                 ManifestMessage(result));
#endif
    return 1;
  }

  int FailRequirementFile(const NativePath &path,
                          std::size_t line,
                          loka::lrpc::RequirementResult result)
  {
    if (result == loka::lrpc::REQUIREMENTS_CANNOT_READ)
    {
      return FailAt(RequirementMessage(result), path);
    }
#if defined(_WIN32)
    std::fwprintf(stderr,
                  L"lrpc: %ls:%lu: %ls\n",
                  path.c_str(),
                  static_cast<unsigned long>(line),
                  RequirementMessage(result));
#else
    std::fprintf(stderr,
                 "lrpc: %s:%lu: %s\n",
                 path.c_str(),
                 static_cast<unsigned long>(line),
                 RequirementMessage(result));
#endif
    return 1;
  }

  int FailRequirements(
      const NativePath &path,
      const std::vector<loka::lrpc::RequirementViolation> &violations)
  {
    for (std::size_t i = 0; i < violations.size(); ++i)
    {
#if defined(_WIN32)
      std::wstring message;
      if (!loka::lrpc::Utf8PathToWide(violations[i].message, message))
      {
        message = L"requirement failed; diagnostic text is not valid UTF-8";
      }
      std::fwprintf(stderr,
                    L"lrpc: %ls:%lu: %ls\n",
                    path.c_str(),
                    static_cast<unsigned long>(violations[i].line),
                    message.c_str());
#else
      std::fprintf(stderr,
                   "lrpc: %s:%lu: %s\n",
                   path.c_str(),
                   static_cast<unsigned long>(violations[i].line),
                   violations[i].message.c_str());
#endif
    }
    return 1;
  }

  int FailIsDirectory(const NativePath &name)
  {
#if defined(_WIN32)
    std::fwprintf(stderr, L"lrpc: %ls is a directory\n", name.c_str());
#else
    std::fprintf(stderr, "lrpc: %s is a directory\n", name.c_str());
#endif
    return 1;
  }

  int FailWouldOverwrite(const NativePath &output, const NativePath &input)
  {
#if defined(_WIN32)
    std::fwprintf(stderr,
                  L"lrpc: %ls would overwrite the input %ls\n",
                  output.c_str(),
                  input.c_str());
#else
    std::fprintf(stderr,
                 "lrpc: %s would overwrite the input %s\n",
                 output.c_str(),
                 input.c_str());
#endif
    return 1;
  }

  int FailSameFile(const NativePath &left, const NativePath &right)
  {
#if defined(_WIN32)
    std::fwprintf(stderr,
                  L"lrpc: %ls and %ls name the same file\n",
                  left.c_str(),
                  right.c_str());
#else
    std::fprintf(stderr,
                 "lrpc: %s and %s name the same file\n",
                 left.c_str(),
                 right.c_str());
#endif
    return 1;
  }

  int FailStampCommit(const NativePath &stampPath,
                      const NativePath &outputPath,
                      bool staleRemoved)
  {
#if defined(_WIN32)
    std::fwprintf(stderr,
                  L"lrpc: cannot commit stamp: %ls\n"
                  L"lrpc: %ls was written; the stamp file is %ls -- read the "
                  L"stamp from the package rather than trusting a stale one\n",
                  stampPath.c_str(),
                  outputPath.c_str(),
                  staleRemoved ? L"removed" : L"possibly stale and could not be removed");
#else
    std::fprintf(stderr,
                 "lrpc: cannot commit stamp: %s\n"
                 "lrpc: %s was written; the stamp file is %s -- read the stamp "
                 "from the package rather than trusting a stale one\n",
                 stampPath.c_str(),
                 outputPath.c_str(),
                 staleRemoved ? "removed" : "possibly stale and could not be removed");
#endif
    return 1;
  }

  void PrintSuccess(const NativePath &outputPath,
                    std::size_t packageSize,
                    std::size_t assetCount,
                    std::size_t bagCount,
                    U32 stamp)
  {
#if defined(_WIN32)
    std::wprintf(L"lrpc: %ls (%lu bytes, %lu assets, %lu bags, stamp %lu)\n",
                 outputPath.c_str(),
                 static_cast<unsigned long>(packageSize),
                 static_cast<unsigned long>(assetCount),
                 static_cast<unsigned long>(bagCount),
                 static_cast<unsigned long>(stamp));
#else
    std::printf("lrpc: %s (%lu bytes, %lu assets, %lu bags, stamp %lu)\n",
                outputPath.c_str(),
                static_cast<unsigned long>(packageSize),
                static_cast<unsigned long>(assetCount),
                static_cast<unsigned long>(bagCount),
                static_cast<unsigned long>(stamp));
#endif
  }

  int Usage()
  {
    std::fprintf(stderr,
                 "usage: lrpc pack <manifest> -o <package> [--stamp <file>] [--require <file> --require-pages <N>]\n"
                 "\n"
                 "  Packs canonical, package-ready asset records into an LRPK\n"
                 "  package. No format conversion happens here: payload bytes\n"
                 "  are already in the form the target consumes.\n"
                 "\n"
                 "  --stamp writes the derived id-space stamp as one decimal\n"
                 "  line, for the application build to check its header against.\n"
                 "  --require checks the manifest against the app's structural\n"
                 "  package expectations before writing either output. A file\n"
                 "  with a pages rule also requires --require-pages.\n");
    return 2;
  }
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t **argv)
#else
int main(int argc, char **argv)
#endif
{
  if (argc < 2 || NativePath(argv[1]) != LRPC_NATIVE_TEXT("pack"))
  {
    return Usage();
  }

  NativePath manifestPath;
  NativePath outputPath;
  NativePath stampPath;
  NativePath requirementPath;
  std::size_t requiredPageCount = 0;
  bool hasRequiredPageCount = false;
  for (int i = 2; i < argc; ++i)
  {
    const NativePath arg = argv[i];
    if (arg == LRPC_NATIVE_TEXT("-o"))
    {
      if (++i >= argc)
      {
        return Usage();
      }
      outputPath = argv[i];
    }
    else if (arg == LRPC_NATIVE_TEXT("--stamp"))
    {
      if (++i >= argc)
      {
        return Usage();
      }
      stampPath = argv[i];
    }
    else if (arg == LRPC_NATIVE_TEXT("--require"))
    {
      if (++i >= argc)
      {
        return Usage();
      }
      requirementPath = argv[i];
    }
    else if (arg == LRPC_NATIVE_TEXT("--require-pages"))
    {
      if (++i >= argc || hasRequiredPageCount
          || !ParseNativeU32(argv[i], requiredPageCount))
      {
        return Usage();
      }
      hasRequiredPageCount = true;
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
  if (manifestPath.empty() || outputPath.empty()
      || (hasRequiredPageCount && requirementPath.empty()))
  {
    return Usage();
  }

  std::vector<unsigned char> manifestBytes;
  if (!loka::lrpc::ReadWholeFile(manifestPath, manifestBytes))
  {
    return FailAt(LRPC_NATIVE_TEXT("cannot read manifest"), manifestPath);
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
    return FailManifest(manifestPath, errorLine, parsed);
  }

  if (!requirementPath.empty())
  {
    std::vector<loka::lrpc::RequirementViolation> violations;
    const loka::lrpc::RequirementResult loaded =
        loka::lrpc::CheckPackageRequirementsFile(
            requirementPath.c_str(),
            manifest,
            hasRequiredPageCount ? &requiredPageCount : 0,
            violations,
            errorLine);
    if (loaded != loka::lrpc::REQUIREMENTS_OK)
    {
      return FailRequirementFile(requirementPath, errorLine, loaded);
    }
    if (!violations.empty())
    {
      return FailRequirements(requirementPath, violations);
    }
  }

  // Both outputs are written after the package is committed, so an output that
  // names an input destroys it, and an output that names the other output
  // truncates whichever landed first -- while the command still prints success
  // and exits zero. Refuse before anything is written.
  //
  // The comparison starts lexically and uses filesystem identity where the
  // host seam below provides it. It catches the collision a build script
  // actually produces, which is usually the same variable used twice.
  // What this guard is for, and where it stops.
  //
  // It is not a correctness property of the package. Every input is read into
  // memory before any output is created, so the bytes written are right no
  // matter what the paths turn out to alias. What the guard protects is the
  // author's source files, from a build script that pointed an output at one
  // of them.
  //
  // That is worth catching cheaply and is not worth chasing to the bottom.
  // Path aliasing has no floor -- case-insensitive volumes, Unicode
  // normalisation, bind mounts, a race between the check and the write -- and
  // this is a build tool invoked with paths a build system generated. The line
  // is drawn deliberately at:
  //
  //   * lexical keys resolved into one namespace, so spellings of one path agree
  //   * filesystem identity for files that exist, so links do not present two
  //   * exclusive creation of the staging files, so they cannot be aliases at all
  //
  // and no further. A case beyond that line is a decided limit, not an
  // oversight. On Windows the lexical keys are wide, so this path-width fix
  // does not also need to become filesystem-identity work.
  //
  // Every path the run touches is checked as two complete lists rather than as a
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
  const NativePath base = DirectoryOf(manifestPath);
  std::vector<NativePath> assetPaths(manifest.assets.size());
  for (std::size_t i = 0; i < manifest.assets.size(); ++i)
  {
    if (!ResolveAssetPath(base, manifest.assets[i].source, assetPaths[i]))
    {
      return Fail("asset source is not valid UTF-8");
    }
  }
  {
    std::vector<NativePath> inputs;
    std::vector<NativePath> inputNames;
    inputs.push_back(CollisionKey(manifestPath));
    inputNames.push_back(manifestPath);
    if (!requirementPath.empty())
    {
      inputs.push_back(CollisionKey(requirementPath));
      inputNames.push_back(requirementPath);
    }
    for (std::size_t i = 0; i < manifest.assets.size(); ++i)
    {
      inputs.push_back(CollisionKey(assetPaths[i]));
      inputNames.push_back(assetPaths[i]);
    }

    std::vector<NativePath> outputs;
    std::vector<NativePath> outputPaths;
    std::vector<NativePath> outputNames;
    // Named by role as well as by path: two outputs that collide often share
    // a spelling, and "Z.tmp and Z.tmp name the same file" tells the author
    // nothing about which two things met there.
    outputPaths.push_back(outputPath);
    outputNames.push_back(LRPC_NATIVE_TEXT("the package (") + outputPath +
                          LRPC_NATIVE_TEXT(")"));
    outputPaths.push_back(outputPath + LRPC_NATIVE_TEXT(".tmp"));
    outputNames.push_back(LRPC_NATIVE_TEXT("the package staging file (") +
                          outputPath + LRPC_NATIVE_TEXT(".tmp)"));
    if (!stampPath.empty())
    {
      outputPaths.push_back(stampPath);
      outputNames.push_back(LRPC_NATIVE_TEXT("the stamp (") + stampPath +
                            LRPC_NATIVE_TEXT(")"));
      outputPaths.push_back(stampPath + LRPC_NATIVE_TEXT(".tmp"));
      outputNames.push_back(LRPC_NATIVE_TEXT("the stamp staging file (") +
                            stampPath + LRPC_NATIVE_TEXT(".tmp)"));
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
        return FailIsDirectory(outputNames[o]);
      }
      for (std::size_t i = 0; i < inputs.size(); ++i)
      {
        if (outputs[o] == inputs[i])
        {
          return FailWouldOverwrite(outputNames[o], inputNames[i]);
        }
      }
      for (std::size_t p = o + 1; p < outputs.size(); ++p)
      {
        if (outputs[o] == outputs[p])
        {
          return FailSameFile(outputNames[o], outputNames[p]);
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
    if (!loka::lrpc::ReadWholeFile(assetPaths[i], payloads[i]))
    {
      return FailAt(LRPC_NATIVE_TEXT("cannot read asset payload"), assetPaths[i]);
    }
    writer.addAsset(loka::lrpc::AssetLayoutKey(asset.source),
                    asset.id,
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
  const NativePath temporary = outputPath + LRPC_NATIVE_TEXT(".tmp");
  const NativePath stampTemporary =
      stampPath.empty() ? NativePath() : stampPath + LRPC_NATIVE_TEXT(".tmp");

  if (!WriteNewFile(temporary, package.empty() ? 0 : &package[0], package.size()))
  {
    return FailAt(LRPC_NATIVE_TEXT("cannot create package staging file"), temporary);
  }
  if (!stampPath.empty())
  {
    char line[32];
    const int printed = std::sprintf(line, "%lu\n", static_cast<unsigned long>(stamp));
    if (printed <= 0 ||
        !WriteNewFile(stampTemporary, reinterpret_cast<const unsigned char *>(line),
                      static_cast<std::size_t>(printed)))
    {
      RemoveFile(temporary);
      return FailAt(LRPC_NATIVE_TEXT("cannot write stamp"), stampTemporary);
    }
  }

  if (!CommitByRename(temporary, outputPath))
  {
    RemoveFile(temporary);
    if (!stampTemporary.empty())
    {
      RemoveFile(stampTemporary);
    }
    return FailAt(LRPC_NATIVE_TEXT("cannot commit package"), outputPath);
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
    RemoveFile(stampTemporary);
    const bool staleRemoved =
        !PathIsDirectory(stampPath) && RemoveFile(stampPath) == 0;
    return FailStampCommit(stampPath, outputPath, staleRemoved);
  }

  PrintSuccess(outputPath,
               package.size(),
               manifest.assets.size(),
               manifest.bags.size(),
               stamp);
  return 0;
}

#undef LRPC_NATIVE_TEXT
