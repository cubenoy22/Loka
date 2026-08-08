#include "ScrapbookPackageTests.hpp"

#include "support/TestVerify.hpp"
#include "../example/ScrapbookUI/src/ScrapbookPackage.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/String.hpp"
#include "core/resource/lrpk/LrpkReader.hpp"
#include "core/resource/lrpk/LrpkStdioByteSource.hpp"
#include "lrpc/PackManifest.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/file/FileIO.hpp"

namespace
{
  const loka::core::resource::lrpk::U32 kScrapbookStamp = 3579051217UL;
  const unsigned char kPngSignature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

  class PackagePathContext : public PlatformContext
  {
  public:
    explicit PackagePathContext(const loka::core::String &path)
        : path_(path)
    {
    }

    virtual App *createApp(AppConfigurable *, HINSTANCE, int) const
    {
      return 0;
    }

    virtual Window *createWindow(const WindowProps &)
    {
      return 0;
    }

    virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *) const
    {
      return 0;
    }

    virtual bool openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const
    {
      if (item.base() != loka::file::File::BASE_APPLICATION
          || !item.relativePath().equals(loka::core::String::Literal("ASSETS.LRP")))
      {
        return false;
      }
      out.displayPath = this->path_;
      return true;
    }

    virtual bool createImageFromBlob(const loka::core::resource::Blob &,
                                     std::size_t,
                                     std::size_t,
                                     loka::core::resource::Image &out) const
    {
      // The package treats its UI image as optional, and the text-page pin
      // needs no fake decoder. Native PNG decoding stays a platform test.
      out = loka::core::resource::Image::Empty();
      return false;
    }

  private:
    loka::core::String path_;
  };

  std::string SourcePath(const char *relative)
  {
    const std::string sourceFile(__FILE__);
    const std::string unixMarker = "/tests/ScrapbookPackageTests.cpp";
    const std::string windowsMarker = "\\tests\\ScrapbookPackageTests.cpp";
    std::string::size_type marker = sourceFile.rfind(unixMarker);
    if (marker == std::string::npos)
    {
      marker = sourceFile.rfind(windowsMarker);
    }
    assert(marker != std::string::npos);
    return sourceFile.substr(0, marker) + "/" + relative;
  }

  bool ReadWholeFile(const std::string &path, std::vector<unsigned char> &out)
  {
    out.clear();
    std::FILE *file = loka::platform::file::OpenRead(loka::core::String::Utf8(path.data(), path.size()));
    if (!file)
    {
      return false;
    }
    if (std::fseek(file, 0, SEEK_END) != 0)
    {
      std::fclose(file);
      return false;
    }
    const long end = std::ftell(file);
    if (end < 0 || std::fseek(file, 0, SEEK_SET) != 0)
    {
      std::fclose(file);
      return false;
    }
    out.resize(static_cast<std::size_t>(end));
    const bool read = out.empty() || std::fread(&out[0], 1, out.size(), file) == out.size();
    std::fclose(file);
    if (!read)
    {
      out.clear();
    }
    return read;
  }
} // namespace

void testScrapbookModernPackageMatchesItsManifestAndCarriesPngImages()
{
  const std::string manifestPath = SourcePath("example/ScrapbookUI/assets/manifest-modern.txt");
  const std::string packagePath = SourcePath("example/ScrapbookUI/assets/ASSETS-modern.LRP");

  std::vector<unsigned char> manifestBytes;
  LOKA_VERIFY(ReadWholeFile(manifestPath, manifestBytes));
  loka::lrpc::PackManifest manifest;
  std::size_t errorLine = 0;
  LOKA_VERIFY(loka::lrpc::ParseManifest(manifestBytes.empty() ? "" : reinterpret_cast<const char *>(&manifestBytes[0]),
                                        manifestBytes.size(),
                                        manifest,
                                        errorLine)
              == loka::lrpc::MANIFEST_OK);
  assert(loka::lrpc::DeriveIdSpaceStamp(manifest) == kScrapbookStamp);

  loka::core::resource::lrpk::StdioByteSource source;
  LOKA_VERIFY(source.open(loka::core::String::Utf8(packagePath.data(), packagePath.size())));
  loka::core::resource::lrpk::Reader reader;
  std::size_t indexBytesNeeded = 0;
  LOKA_VERIFY(
      reader.beginOpen(source, kScrapbookStamp, loka::core::resource::lrpk::Reader::VERIFY_INTEGRITY, indexBytesNeeded)
      == loka::core::resource::lrpk::Reader::OPEN_OK);
  std::vector<unsigned char> index(indexBytesNeeded);
  LOKA_VERIFY(reader.finishOpen(index.empty() ? 0 : &index[0], index.size())
              == loka::core::resource::lrpk::Reader::OPEN_OK);
  assert(reader.bagCount() == manifest.bags.size());
  assert(reader.assetCount() == manifest.assets.size());

  for (std::size_t i = 0; i < manifest.assets.size(); ++i)
  {
    const loka::lrpc::ManifestAsset &expected = manifest.assets[i];
    std::size_t bagSize = 0;
    LOKA_VERIFY(reader.bagStoredSize(expected.bag, bagSize));
    std::vector<unsigned char> bag(bagSize);
    LOKA_VERIFY(reader.readBagInto(expected.bag, bag.empty() ? 0 : &bag[0], bag.size())
                == loka::core::resource::lrpk::Reader::BAG_OK);

    loka::core::resource::lrpk::Facts facts;
    loka::core::resource::lrpk::Asset asset;
    LOKA_VERIFY(reader.get(expected.id, facts, asset) == loka::core::resource::lrpk::Reader::GET_OK);
    assert(asset.bag == expected.bag);
    assert(asset.kind == expected.kind);
    assert(asset.offsetInBag <= bag.size());
    assert(asset.length <= bag.size() - asset.offsetInBag);
    if (expected.kind == loka::core::resource::lrpk::ASSET_KIND_IMAGE)
    {
      assert(asset.length >= sizeof(kPngSignature));
      assert(std::memcmp(&bag[asset.offsetInBag], kPngSignature, sizeof(kPngSignature)) == 0);
    }
    reader.closeBag(expected.bag);
  }

  reader.close();
  source.close();

  const loka::core::String packageString = loka::core::String::Utf8(packagePath.data(), packagePath.size());
  PackagePathContext context(packageString);
  scrapbook::ScrapbookPackage package;
  LOKA_VERIFY(package.open(&context));
  scrapbook::PagePresentation textPage;
  LOKA_VERIFY(package.preparePage(4, textPage));
  assert(!textPage.isImage);
  assert(textPage.badge.equals(loka::core::String::Literal("TEXT")));
  assert(textPage.text.equals(loka::core::String::Literal("The Scrapbook keeps each page in its own LRPK bag.\n")));
  package.commitPage(textPage);
  assert(package.hasCurrentPage());
  assert(package.currentPage() == 4);
  package.close();

  std::printf("testScrapbookModernPackageMatchesItsManifestAndCarriesPngImages passed\n");
}
