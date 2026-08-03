#include "PackManifestTests.hpp"
#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "lrpc/PackManifest.hpp"

using loka::lrpc::DeriveIdSpaceStamp;
using loka::lrpc::ManifestResult;
using loka::lrpc::PackManifest;
using loka::lrpc::ParseManifest;

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
} // namespace

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

  // The symbolic name is part of the association the stamp guards, so renaming
  // a symbol restamps.
  PackManifest renamed;
  LOKA_VERIFY(Parse("bag Main\nasset 1001 image Renamed a\nasset 2001 string B b\n",
               renamed,
               line) == loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(renamed) != DeriveIdSpaceStamp(listedOneWay));

  // The case that makes hashing the name load-bearing rather than tidy: two
  // same-kind symbols exchange ids. The sorted `(id, kind)` multiset is
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
