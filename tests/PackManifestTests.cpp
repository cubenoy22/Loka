#include "PackManifestTests.hpp"
#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "lrpc/PackManifest.hpp"
#include "lrpc/ResourceHeader.hpp"

using loka::lrpc::DeriveIdSpaceStamp;
using loka::lrpc::ManifestResult;
using loka::lrpc::PackManifest;
using loka::lrpc::ParseManifest;
using loka::lrpc::RequirementResult;
using loka::lrpc::RequirementViolation;
using loka::lrpc::ResourceHeaderResult;

namespace
{
  ManifestResult Parse(const char *text, PackManifest &out, std::size_t &line)
  {
    return ParseManifest(text, std::strlen(text), out, line);
  }

  ManifestResult Refusal(const char *text, std::size_t &line)
  {
    PackManifest ignored;
    return Parse(text, ignored, line);
  }

  PackManifest Manifest(const char *text)
  {
    PackManifest manifest;
    std::size_t line = 0;
    LOKA_VERIFY(Parse(text, manifest, line) == loka::lrpc::MANIFEST_OK);
    return manifest;
  }
} // namespace

void testResourceHeaderGeneratesTypedNestedSymbols()
{
  const PackManifest manifest = Manifest(
      "bag ui\n"
      "asset 9001 image UI/RefusedBadge badge.pict\n"
      "asset 1001 image Pages/One one.pict\n"
      "bag text\n"
      "asset 1002 string Pages/Two two.txt\n"
      "asset 9002 string UI/Caption caption.txt\n");

  std::string header;
  LOKA_VERIFY(loka::lrpc::GenerateResourceHeader(manifest, header) ==
              loka::lrpc::RESOURCE_HEADER_OK);
  assert(header.find("struct AssetRef") != std::string::npos);
  assert(header.find("const AssetId IdSpaceStamp = ") != std::string::npos);
  assert(header.find("const std::size_t AssetCount = 4;") != std::string::npos);
  assert(header.find("const std::size_t BagCount = 2;") != std::string::npos);
  assert(header.find("namespace UI") != std::string::npos);
  assert(header.find("const AssetRef RefusedBadge = {9001UL, 0, ") !=
         std::string::npos);
  assert(header.find("const AssetRef Caption = {9002UL, 1, ") !=
         std::string::npos);
  assert(header.find("namespace Pages") != std::string::npos);
  assert(header.find("const AssetRef One = {1001UL, 0, ") != std::string::npos);
  assert(header.find("const AssetRef Two = {1002UL, 1, ") != std::string::npos);
  // Interleaved manifest records still form one namespace-owned sequence.
  const std::string uiOpening("  namespace UI\n  {");
  assert(header.find(uiOpening) == header.rfind(uiOpening));
  assert(header.find("const std::size_t AssetCount = 2;", header.find(uiOpening)) !=
         std::string::npos);

  const PackManifest renamedSources = Manifest(
      "bag ui\n"
      "asset 9001 image UI/RefusedBadge elsewhere.png\n"
      "asset 1001 image Pages/One elsewhere-one.png\n"
      "bag text\n"
      "asset 1002 string Pages/Two elsewhere-two.txt\n"
      "asset 9002 string UI/Caption elsewhere-caption.txt\n");
  std::string sameHeader;
  LOKA_VERIFY(loka::lrpc::GenerateResourceHeader(renamedSources, sameHeader) ==
              loka::lrpc::RESOURCE_HEADER_OK);
  assert(header == sameHeader);

  const PackManifest oneListing = Manifest(
      "bag all\n"
      "asset 7 image UI/Icon icon\n"
      "asset 3 string Pages/First first\n"
      "asset 5 string Pages/Second second\n");
  const PackManifest anotherListing = Manifest(
      "bag all\n"
      "asset 5 string Pages/Second second\n"
      "asset 7 image UI/Icon icon\n"
      "asset 3 string Pages/First first\n");
  std::string canonicalOne;
  std::string canonicalOther;
  LOKA_VERIFY(loka::lrpc::GenerateResourceHeader(oneListing, canonicalOne) ==
              loka::lrpc::RESOURCE_HEADER_OK);
  LOKA_VERIFY(loka::lrpc::GenerateResourceHeader(anotherListing, canonicalOther) ==
              loka::lrpc::RESOURCE_HEADER_OK);
  assert(canonicalOne == canonicalOther);
  assert(canonicalOne.find("const AssetRef First") <
         canonicalOne.find("const AssetRef Second"));

  std::printf("testResourceHeaderGeneratesTypedNestedSymbols passed\n");
}

void testResourceHeaderRefusesAmbiguousCppSurfaces()
{
  const char *badSymbols[] = {
      "bag Main\nasset 1 image 9Lives a\n",
      "bag Main\nasset 1 image UI/class a\n",
      "bag Main\nasset 1 image UI/concept a\n",
      "bag Main\nasset 1 image UI/Bad__Name a\n",
      "bag Main\nasset 1 image UI/Bad-Name a\n"};
  for (std::size_t i = 0; i < sizeof(badSymbols) / sizeof(badSymbols[0]); ++i)
  {
    std::string preserved("preserved");
    LOKA_VERIFY(loka::lrpc::GenerateResourceHeader(Manifest(badSymbols[i]), preserved) ==
                loka::lrpc::RESOURCE_HEADER_BAD_SYMBOL);
    assert(preserved == "preserved");
  }

  std::string preserved("preserved");
  LOKA_VERIFY(loka::lrpc::GenerateResourceHeader(
                  Manifest("bag Main\nasset 1 image UI/Badge a\n"
                           "asset 2 image UI/Badge/Small b\n"),
                  preserved) == loka::lrpc::RESOURCE_HEADER_SYMBOL_COLLISION);
  assert(preserved == "preserved");

  LOKA_VERIFY(loka::lrpc::GenerateResourceHeader(
                  Manifest("bag Main\nasset 1 image Pages/Assets a\n"),
                  preserved) == loka::lrpc::RESOURCE_HEADER_RESERVED_SYMBOL);
  assert(preserved == "preserved");

  std::printf("testResourceHeaderRefusesAmbiguousCppSurfaces passed\n");
}

void testResourceHeaderHandlesFiftyThousandSymbols()
{
  PackManifest manifest;
  manifest.bags.push_back("bulk");
  manifest.assets.resize(50000);
  for (std::size_t i = 0; i < manifest.assets.size(); ++i)
  {
    char name[40];
    std::sprintf(name, "Bulk/A%lu", static_cast<unsigned long>(i + 1));
    manifest.assets[i].id = static_cast<loka::core::resource::lrpk::U32>(i + 1);
    manifest.assets[i].kind = loka::core::resource::lrpk::ASSET_KIND_IMAGE;
    manifest.assets[i].bag = 0;
    manifest.assets[i].name = name;
  }

  std::string header;
  LOKA_VERIFY(loka::lrpc::GenerateResourceHeader(manifest, header) ==
              loka::lrpc::RESOURCE_HEADER_OK);
  assert(header.find("const std::size_t AssetCount = 50000;") !=
         std::string::npos);
  assert(header.find("const AssetRef A50000 = {50000UL, 0,") !=
         std::string::npos);

  std::printf("testResourceHeaderHandlesFiftyThousandSymbols passed\n");
}

void testPackManifestParsesRecordsAndRefusesMalformedLines()
{
  PackManifest manifest;
  std::size_t line = 0;
  LOKA_VERIFY(Parse("# a comment\n"
               "bag Main\n"
               "asset 1001 image  Splash/Logo    logo.pict   # trailing comment\n"
               "\n"
               "bag Strings\n"
               "asset 2001 string Hello/Greeting hello.txt",
               manifest,
               line) == loka::lrpc::MANIFEST_OK);
  assert(manifest.bags.size() == 2);
  assert(manifest.bags[0] == "Main" && manifest.bags[1] == "Strings");
  assert(manifest.assets.size() == 2);

  assert(manifest.assets[0].id == 1001);
  assert(manifest.assets[0].kind == loka::core::resource::lrpk::ASSET_KIND_IMAGE);
  assert(manifest.assets[0].name == "Splash/Logo");
  assert(manifest.assets[0].source == "logo.pict");
  // An asset joins the most recently declared bag, so bag membership is
  // positional rather than named on every row.
  assert(manifest.assets[0].bag == 0);
  assert(manifest.assets[1].bag == 1);
  // The final line has no newline: a manifest that a text editor did not
  // terminate must still parse rather than silently losing its last asset.
  assert(manifest.assets[1].id == 2001);
  assert(manifest.assets[1].kind == loka::core::resource::lrpk::ASSET_KIND_STRING);

  // Every refusal reports the 1-based line, because a build tool that says
  // only "bad manifest" makes the author bisect the file by hand.
  LOKA_VERIFY(Refusal("bag Main\nasset 1 image N s\nbogus x\n", line) ==
         loka::lrpc::MANIFEST_UNKNOWN_DIRECTIVE);
  assert(line == 3);
  LOKA_VERIFY(Refusal("bag Main\nasset 1 image N\n", line) == loka::lrpc::MANIFEST_BAD_FIELD_COUNT);
  assert(line == 2);
  LOKA_VERIFY(Refusal("bag Main\nasset 0x10 image N s\n", line) == loka::lrpc::MANIFEST_BAD_ID);
  assert(line == 2);
  LOKA_VERIFY(Refusal("bag Main\nasset 4294967296 image N s\n", line) == loka::lrpc::MANIFEST_BAD_ID);
  assert(line == 2);
  // ASSET_KIND_UNKNOWN has no spelling: the writer refuses that row, so the
  // manifest must not be able to ask for it.
  LOKA_VERIFY(Refusal("bag Main\nasset 1 unknown N s\n", line) == loka::lrpc::MANIFEST_BAD_KIND);
  assert(line == 2);
  LOKA_VERIFY(Refusal("asset 1 image N s\n", line) == loka::lrpc::MANIFEST_ASSET_BEFORE_BAG);
  assert(line == 1);
  LOKA_VERIFY(Refusal("bag Main\nasset 1 image A a\nasset 1 string B b\n", line) ==
         loka::lrpc::MANIFEST_DUPLICATE_ID);
  assert(line == 3);
  LOKA_VERIFY(Refusal("bag Main\nbag Main\n", line) == loka::lrpc::MANIFEST_DUPLICATE_BAG);
  assert(line == 2);
  LOKA_VERIFY(Refusal("bag Main\n", line) == loka::lrpc::MANIFEST_EMPTY);
  LOKA_VERIFY(Refusal("", line) == loka::lrpc::MANIFEST_EMPTY);

  // Two rows cannot share a symbolic name: the name-to-id association has to
  // be a function for a header to be generated from it, and the stamp hashes
  // that association.
  LOKA_VERIFY(Refusal("bag Main\nasset 1 image Same a\nasset 2 image Same b\n", line) ==
         loka::lrpc::MANIFEST_DUPLICATE_NAME);
  assert(line == 3);

  // A NUL is refused for the whole file rather than per field. Carried into a
  // source path it is not inert: every comparison in the tool sees the full
  // string while `fopen` stops at the NUL, so `payload\0suffix` is read from
  // `payload` while the collision guard believes another file was named --
  // which is how an output silently replaces an input it never noticed.
  {
    const char nulManifest[] = "bag Main\nasset 1 image A payload\0suffix\n";
    PackManifest ignored;
    std::size_t nulLine = 0;
    LOKA_VERIFY(ParseManifest(nulManifest, sizeof(nulManifest) - 1, ignored, nulLine) ==
           loka::lrpc::MANIFEST_EMBEDDED_NUL);
  }

  std::printf("testPackManifestParsesRecordsAndRefusesMalformedLines passed\n");
}

void testPackManifestStampFollowsTheIdSpaceNotTheListing()
{
  PackManifest listedOneWay;
  PackManifest listedTheOther;
  std::size_t line = 0;
  LOKA_VERIFY(Parse("bag Main\nasset 1001 image A a\nasset 2001 string B b\n", listedOneWay, line) ==
         loka::lrpc::MANIFEST_OK);
  LOKA_VERIFY(Parse("bag Main\nasset 2001 string B b\nasset 1001 image A a\n", listedTheOther, line) ==
         loka::lrpc::MANIFEST_OK);
  // Reordering the manifest does not move an id, so it must not restamp the
  // package -- otherwise every reshuffle would look like a rebuild to the
  // application's stamp check.
  assert(DeriveIdSpaceStamp(listedOneWay) == DeriveIdSpaceStamp(listedTheOther));

  // The whole point of deriving rather than hand-typing the stamp (#185 §10):
  // a renumbered id must change it, or the mismatch it exists to catch stays
  // silent while every lookup has moved.
  PackManifest renumbered;
  LOKA_VERIFY(Parse("bag Main\nasset 1002 image A a\nasset 2001 string B b\n", renumbered, line) ==
         loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(renumbered) != DeriveIdSpaceStamp(listedOneWay));

  // Kind is part of the id space: the same number meaning a string instead of
  // an image is a different header, and the application would read the wrong
  // accessor type against a package that otherwise passed every check.
  PackManifest retyped;
  LOKA_VERIFY(Parse("bag Main\nasset 1001 string A a\nasset 2001 string B b\n", retyped, line) ==
         loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(retyped) != DeriveIdSpaceStamp(listedOneWay));

  // Source paths are build-side bookkeeping and never reach the application,
  // so renaming a file on disk must not invalidate every header.
  PackManifest resourced;
  LOKA_VERIFY(Parse("bag Main\nasset 1001 image A other.pict\nasset 2001 string B b\n",
               resourced,
               line) == loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(resourced) == DeriveIdSpaceStamp(listedOneWay));

  // Bag membership is generated into AssetRef. An older header that opens
  // the previous bag must therefore be rejected even though the id, kind, and
  // name did not move.
  PackManifest movedBag;
  LOKA_VERIFY(Parse("bag Main\nasset 1001 image A a\nbag Later\nasset 2001 string B b\n",
                    movedBag,
                    line) == loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(movedBag) != DeriveIdSpaceStamp(listedOneWay));

  // BagCount is also emitted in R.hpp. Even an appended empty bag has to move
  // the stamp or a stale header would pass the identity check and fail later.
  PackManifest extraEmptyBag;
  LOKA_VERIFY(Parse("bag Main\nasset 1001 image A a\nasset 2001 string B b\nbag Empty\n",
                    extraEmptyBag,
                    line) == loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(extraEmptyBag) != DeriveIdSpaceStamp(listedOneWay));

  // The symbolic name is part of the association the stamp guards, so renaming
  // a symbol restamps.
  PackManifest renamed;
  LOKA_VERIFY(Parse("bag Main\nasset 1001 image Renamed a\nasset 2001 string B b\n",
               renamed,
               line) == loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(renamed) != DeriveIdSpaceStamp(listedOneWay));

  // The case that makes hashing the name load-bearing rather than tidy: two
  // same-kind symbols exchange ids. The sorted `(id, kind, bag)` multiset is
  // identical, so a stamp over ids alone does not move -- while the generated
  // header now points each symbol at the other's bytes and an old package
  // passes every check and draws the wrong asset.
  PackManifest straight;
  PackManifest swapped;
  LOKA_VERIFY(Parse("bag Main\nasset 1001 image Splash/Logo l.pict\nasset 2001 image Icons/Gear g.pict\n",
               straight,
               line) == loka::lrpc::MANIFEST_OK);
  LOKA_VERIFY(Parse("bag Main\nasset 2001 image Splash/Logo l.pict\nasset 1001 image Icons/Gear g.pict\n",
               swapped,
               line) == loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(straight) != DeriveIdSpaceStamp(swapped));

  // Length-prefixing, so two names cannot be re-cut into the same byte stream.
  PackManifest cutOneWay;
  PackManifest cutTheOther;
  LOKA_VERIFY(Parse("bag Main\nasset 1 image AB a\nasset 2 image C b\n", cutOneWay, line) ==
         loka::lrpc::MANIFEST_OK);
  LOKA_VERIFY(Parse("bag Main\nasset 1 image A a\nasset 2 image BC b\n", cutTheOther, line) ==
         loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(cutOneWay) != DeriveIdSpaceStamp(cutTheOther));

  std::printf("testPackManifestStampFollowsTheIdSpaceNotTheListing passed\n");
}

void testPackageRequirementsCheckEveryScrapbookExpectation()
{
  std::vector<RequirementViolation> violations;
  const std::size_t pageCount = 2;

  const char *all =
      "bag 0 ui\n"
      "asset 9001 image in ui\n"
      "pages 1001 count-from bag 1 kinds image,string\n";
  const PackManifest satisfied = Manifest(
      "bag ui\n"
      "asset 9001 image UI/Badge badge\n"
      "bag page-1\n"
      "asset 1001 image Page/One one\n"
      "bag page-2\n"
      "asset 1002 string Page/Two two\n");
  std::size_t line = 0;
  LOKA_VERIFY(loka::lrpc::CheckPackageRequirements(
                  all, std::strlen(all), satisfied, &pageCount, violations, line) ==
              loka::lrpc::REQUIREMENTS_OK);
  assert(violations.empty());

  const PackManifest allWrong = Manifest(
      "bag pages\n"
      "asset 1001 image Page/One one\n"
      "bag page-1\n"
      "bag page-2\n"
      "asset 1002 string Page/Two two\n");
  LOKA_VERIFY(loka::lrpc::CheckPackageRequirements(
                  all, std::strlen(all), allWrong, &pageCount, violations, line) ==
              loka::lrpc::REQUIREMENTS_OK);
  assert(violations.size() == 4);
  assert(violations[0].line == 1);
  assert(violations[0].message ==
         "bag 0 must be named \"ui\"; found bag 0 named \"pages\"");
  assert(violations[1].line == 2);
  assert(violations[1].message ==
         "asset 9001 must have kind image; found no asset with that id");
  assert(violations[2].line == 3);
  assert(violations[2].message ==
         "pages from asset 1001 must occupy 2 bags from 1 one per bag; "
         "found asset 1001 in bag 0 (\"pages\"), expected bag 1 (\"page-1\")");
  assert(violations[3].line == 3);
  assert(violations[3].message ==
         "package must contain exactly 3 assets (2 pages plus 1 listed asset); found 2");

  const char *asset = "asset 9001 image in ui\n";
  const PackManifest wrongKind = Manifest(
      "bag ui\n"
      "asset 9001 string UI/Badge badge\n");
  LOKA_VERIFY(loka::lrpc::CheckPackageRequirements(
                  asset, std::strlen(asset), wrongKind, 0, violations, line) ==
              loka::lrpc::REQUIREMENTS_OK);
  assert(violations.size() == 1);
  assert(violations[0].line == 1);
  assert(violations[0].message ==
         "asset 9001 must have kind image; found kind string");

  std::printf("testPackageRequirementsCheckEveryScrapbookExpectation passed\n");
}

void testPackageRequirementsRejectWrongPageCount()
{
  const char *requirements =
      "bag 0 ui\n"
      "asset 9001 image in ui\n"
      "pages 1001 count-from bag 1 kinds image,string\n";
  const PackManifest fourPages = Manifest(
      "bag ui\n"
      "asset 9001 image UI/Badge badge\n"
      "bag page-1\n"
      "asset 1001 image Page/One one\n"
      "bag page-2\n"
      "asset 1002 image Page/Two two\n"
      "bag page-3\n"
      "asset 1003 string Page/Three three\n"
      "bag page-4\n"
      "asset 1004 image Page/Four four\n");
  std::vector<RequirementViolation> violations;
  std::size_t line = 0;
  const std::size_t pageCount = 5;
  LOKA_VERIFY(loka::lrpc::CheckPackageRequirements(
                  requirements,
                  std::strlen(requirements),
                  fourPages,
                  &pageCount,
                  violations,
                  line) == loka::lrpc::REQUIREMENTS_OK);
  assert(violations.size() == 2);
  assert(violations[0].line == 3);
  assert(violations[0].message ==
         "pages from asset 1001 must occupy 5 bags from 1 one per bag; "
         "found 4 bags from index 1");
  assert(violations[1].line == 3);
  assert(violations[1].message ==
         "package must contain exactly 6 assets (5 pages plus 1 listed asset); found 5");

  LOKA_VERIFY(loka::lrpc::CheckPackageRequirements(
                  requirements,
                  std::strlen(requirements),
                  fourPages,
                  0,
                  violations,
                  line) == loka::lrpc::REQUIREMENTS_PAGE_COUNT_REQUIRED);
  assert(line == 3);

  std::printf("testPackageRequirementsRejectWrongPageCount passed\n");
}

void testPackageRequirementsRejectExtraAsset()
{
  const char *requirements =
      "asset 9001 image in ui\n"
      "pages 1001 count-from bag 1 kinds image,string\n";
  const PackManifest extraAsset = Manifest(
      "bag ui\n"
      "asset 9001 image UI/Badge badge\n"
      "asset 7001 string Unlisted unlisted\n"
      "bag page-1\n"
      "asset 1001 image Page/One one\n"
      "bag page-2\n"
      "asset 1002 string Page/Two two\n");
  std::vector<RequirementViolation> violations;
  std::size_t line = 0;
  const std::size_t pageCount = 2;
  LOKA_VERIFY(loka::lrpc::CheckPackageRequirements(
                  requirements,
                  std::strlen(requirements),
                  extraAsset,
                  &pageCount,
                  violations,
                  line) == loka::lrpc::REQUIREMENTS_OK);
  assert(violations.size() == 1);
  assert(violations[0].line == 2);
  assert(violations[0].message ==
         "package must contain exactly 3 assets (2 pages plus 1 listed asset); found 4");

  std::printf("testPackageRequirementsRejectExtraAsset passed\n");
}

void testPackageRequirementsRejectBadPageKind()
{
  const char *requirements =
      "asset 9001 image in ui\n"
      "pages 1001 count-from bag 1 kinds image,string\n";
  const PackManifest audioPage = Manifest(
      "bag ui\n"
      "asset 9001 image UI/Badge badge\n"
      "bag page-1\n"
      "asset 1001 image Page/One one\n"
      "bag page-2\n"
      "asset 1002 audio Page/Two two\n");
  std::vector<RequirementViolation> violations;
  std::size_t line = 0;
  const std::size_t pageCount = 2;
  LOKA_VERIFY(loka::lrpc::CheckPackageRequirements(
                  requirements,
                  std::strlen(requirements),
                  audioPage,
                  &pageCount,
                  violations,
                  line) == loka::lrpc::REQUIREMENTS_OK);
  assert(violations.size() == 1);
  assert(violations[0].line == 2);
  assert(violations[0].message ==
         "page asset 1002 must have one of kinds image,string; found kind audio");

  std::printf("testPackageRequirementsRejectBadPageKind passed\n");
}

void testPackageRequirementsRejectAssetOutsideNamedBag()
{
  const char *requirements = "asset 9001 image in ui\n";
  const PackManifest movedBadge = Manifest(
      "bag ui\n"
      "bag page-1\n"
      "asset 9001 image UI/Badge badge\n");
  std::vector<RequirementViolation> violations;
  std::size_t line = 0;
  LOKA_VERIFY(loka::lrpc::CheckPackageRequirements(
                  requirements,
                  std::strlen(requirements),
                  movedBadge,
                  0,
                  violations,
                  line) == loka::lrpc::REQUIREMENTS_OK);
  assert(violations.size() == 1);
  assert(violations[0].line == 1);
  assert(violations[0].message ==
         "asset 9001 must be in bag \"ui\"; found bag 1 (\"page-1\")");

  std::printf("testPackageRequirementsRejectAssetOutsideNamedBag passed\n");
}

void testPackageRequirementsUnreadableFileIsAHardError()
{
  const PackManifest manifest = Manifest(
      "bag ui\n"
      "asset 1 image Any any\n");
  std::vector<RequirementViolation> violations;
  std::size_t line = 99;
#if defined(_WIN32)
  const RequirementResult result =
      loka::lrpc::CheckPackageRequirementsFile(
          L".", manifest, 0, violations, line);
#else
  const RequirementResult result =
      loka::lrpc::CheckPackageRequirementsFile(
          ".", manifest, 0, violations, line);
#endif
  LOKA_VERIFY(result == loka::lrpc::REQUIREMENTS_CANNOT_READ);
  assert(line == 0);

  std::printf("testPackageRequirementsUnreadableFileIsAHardError passed\n");
}
