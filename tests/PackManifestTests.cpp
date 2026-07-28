#include "PackManifestTests.hpp"

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
  assert(Parse("# a comment\n"
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
  assert(Refusal("bag Main\nasset 1 image N s\nbogus x\n", line) ==
         loka::lrpc::MANIFEST_UNKNOWN_DIRECTIVE);
  assert(line == 3);
  assert(Refusal("bag Main\nasset 1 image N\n", line) == loka::lrpc::MANIFEST_BAD_FIELD_COUNT);
  assert(line == 2);
  assert(Refusal("bag Main\nasset 0x10 image N s\n", line) == loka::lrpc::MANIFEST_BAD_ID);
  assert(line == 2);
  assert(Refusal("bag Main\nasset 4294967296 image N s\n", line) == loka::lrpc::MANIFEST_BAD_ID);
  assert(line == 2);
  // ASSET_KIND_UNKNOWN has no spelling: the writer refuses that row, so the
  // manifest must not be able to ask for it.
  assert(Refusal("bag Main\nasset 1 unknown N s\n", line) == loka::lrpc::MANIFEST_BAD_KIND);
  assert(line == 2);
  assert(Refusal("asset 1 image N s\n", line) == loka::lrpc::MANIFEST_ASSET_BEFORE_BAG);
  assert(line == 1);
  assert(Refusal("bag Main\nasset 1 image A a\nasset 1 string B b\n", line) ==
         loka::lrpc::MANIFEST_DUPLICATE_ID);
  assert(line == 3);
  assert(Refusal("bag Main\nbag Main\n", line) == loka::lrpc::MANIFEST_DUPLICATE_BAG);
  assert(line == 2);
  assert(Refusal("bag Main\n", line) == loka::lrpc::MANIFEST_EMPTY);
  assert(Refusal("", line) == loka::lrpc::MANIFEST_EMPTY);

  std::printf("testPackManifestParsesRecordsAndRefusesMalformedLines passed\n");
}

void testPackManifestStampFollowsTheIdSpaceNotTheListing()
{
  PackManifest listedOneWay;
  PackManifest listedTheOther;
  std::size_t line = 0;
  assert(Parse("bag Main\nasset 1001 image A a\nasset 2001 string B b\n", listedOneWay, line) ==
         loka::lrpc::MANIFEST_OK);
  assert(Parse("bag Main\nasset 2001 string B b\nasset 1001 image A a\n", listedTheOther, line) ==
         loka::lrpc::MANIFEST_OK);
  // Reordering the manifest does not move an id, so it must not restamp the
  // package -- otherwise every reshuffle would look like a rebuild to the
  // application's stamp check.
  assert(DeriveIdSpaceStamp(listedOneWay) == DeriveIdSpaceStamp(listedTheOther));

  // The whole point of deriving rather than hand-typing the stamp (#185 §10):
  // a renumbered id must change it, or the mismatch it exists to catch stays
  // silent while every lookup has moved.
  PackManifest renumbered;
  assert(Parse("bag Main\nasset 1002 image A a\nasset 2001 string B b\n", renumbered, line) ==
         loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(renumbered) != DeriveIdSpaceStamp(listedOneWay));

  // Kind is part of the id space: the same number meaning a string instead of
  // an image is a different header, and the application would read the wrong
  // accessor type against a package that otherwise passed every check.
  PackManifest retyped;
  assert(Parse("bag Main\nasset 1001 string A a\nasset 2001 string B b\n", retyped, line) ==
         loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(retyped) != DeriveIdSpaceStamp(listedOneWay));

  // Names and sources are build-side bookkeeping, not the id space. Renaming a
  // source file must not force every application header to be rebuilt.
  PackManifest renamed;
  assert(Parse("bag Main\nasset 1001 image Other other.pict\nasset 2001 string B b\n",
               renamed,
               line) == loka::lrpc::MANIFEST_OK);
  assert(DeriveIdSpaceStamp(renamed) == DeriveIdSpaceStamp(listedOneWay));

  std::printf("testPackManifestStampFollowsTheIdSpaceNotTheListing passed\n");
}
