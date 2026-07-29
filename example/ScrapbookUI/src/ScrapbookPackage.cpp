#include "ScrapbookPackage.hpp"

#include "core/io/File.hpp"
#include "platform/file/FileHandle.hpp"

namespace scrapbook
{
  using loka::core::resource::Blob;
  using loka::core::resource::Image;
  using loka::core::resource::lrpk::Asset;
  using loka::core::resource::lrpk::Facts;
  using loka::core::resource::lrpk::Reader;

  ScrapbookPackage::ScrapbookPackage()
      : context_(0),
        source_(),
        reader_(),
        indexBytes_(),
        currentBlob_(),
        currentBag_(-1),
        open_(false)
  {
  }

  ScrapbookPackage::~ScrapbookPackage()
  {
    this->close();
  }

  bool ScrapbookPackage::open(PlatformContext *context)
  {
    this->close();
    if (!context)
    {
      return false;
    }

    const loka::file::File item = loka::file::File::Application() << loka::file::File("ASSETS.LRP");
    loka::platform::file::FileHandle handle;
    if (!context->openFile(item, handle) || !handle.hasSpec)
    {
      return false;
    }
    if (!this->source_.open(handle.spec))
    {
      return false;
    }

    std::size_t indexBytesNeeded = 0;
    if (this->reader_.beginOpen(this->source_, kIdSpaceStamp, Reader::VERIFY_INTEGRITY, indexBytesNeeded)
        != Reader::OPEN_OK)
    {
      this->source_.close();
      return false;
    }

    this->indexBytes_.resize(indexBytesNeeded);
    unsigned char *indexBase = this->indexBytes_.empty() ? 0 : &this->indexBytes_[0];
    if (this->reader_.finishOpen(indexBase, this->indexBytes_.size()) != Reader::OPEN_OK
        || this->reader_.bagCount() != kPageCount || this->reader_.assetCount() != kPageCount)
    {
      this->reader_.close();
      this->indexBytes_.clear();
      this->source_.close();
      return false;
    }

    this->context_ = context;
    this->open_ = true;
    return true;
  }

  bool ScrapbookPackage::preparePage(int page, PagePresentation &out)
  {
    out = PagePresentation();
    if (!this->open_ || page < 0 || static_cast<std::size_t>(page) >= kPageCount)
    {
      return false;
    }

    const std::size_t bag = static_cast<std::size_t>(page);
    bool openedNew = false;
    Blob blob;
    if (page == this->currentBag_ && this->reader_.isBagOpen(bag))
    {
      blob = this->currentBlob_;
    }
    else
    {
      std::size_t bagSize = 0;
      if (!this->reader_.bagStoredSize(bag, bagSize))
      {
        return false;
      }

      blob = Blob::Create();
      std::vector<unsigned char> &bytes = blob.mutableBytes();
      bytes.resize(bagSize);
      unsigned char *destination = bytes.empty() ? 0 : &bytes[0];
      if (this->reader_.readBagInto(bag, destination, bytes.size()) != Reader::BAG_OK)
      {
        return false;
      }
      openedNew = true;
      blob.sealBytes();
    }

    if (!this->buildPresentation(page, blob, out))
    {
      this->rollbackPreparedBag(bag, openedNew);
      return false;
    }
    return true;
  }

  bool ScrapbookPackage::buildPresentation(int page, const Blob &blob, PagePresentation &out)
  {
    Facts facts;
    Asset asset;
    if (this->reader_.get(PageAssetId(static_cast<std::size_t>(page)), facts, asset) != Reader::GET_OK
        || asset.bag != static_cast<std::size_t>(page))
    {
      return false;
    }

    const bool imagePage = page < 4;
    const loka::core::resource::lrpk::AssetKind expectedKind =
        imagePage ? loka::core::resource::lrpk::ASSET_KIND_IMAGE : loka::core::resource::lrpk::ASSET_KIND_STRING;
    if (asset.kind != expectedKind)
    {
      return false;
    }

    PagePresentation next;
    next.bag = asset.bag;
    next.bagBlob = blob;
    next.caption = loka::core::String::FromInt(page + 1) + loka::core::String::Literal(" / 5");
    next.badge = loka::core::String::Literal(imagePage ? "PICT" : "TEXT");
    next.isImage = imagePage;

    if (imagePage)
    {
      if (!this->context_->createImageFromBlob(blob, asset.offsetInBag, asset.length, next.image))
      {
        return false;
      }
    }
    else
    {
      const char *textBytes = asset.length == 0 ? "" : reinterpret_cast<const char *>(&blob.bytes()[asset.offsetInBag]);
      next.text = loka::core::String::Utf8(textBytes, asset.length);
    }

    out = next;
    return true;
  }

  void ScrapbookPackage::commitPage(const PagePresentation &page)
  {
    const int nextBag = static_cast<int>(page.bag);
    if (nextBag == this->currentBag_)
    {
      return;
    }

    const int previousBag = this->currentBag_;
    if (previousBag >= 0)
    {
      this->reader_.closeBag(static_cast<std::size_t>(previousBag));
    }
    this->currentBlob_ = page.bagBlob;
    this->currentBag_ = nextBag;
  }

  void ScrapbookPackage::rollbackPreparedBag(std::size_t bag, bool openedNew)
  {
    if (openedNew)
    {
      this->reader_.closeBag(bag);
    }
  }

  void ScrapbookPackage::close()
  {
    this->reader_.close();
    this->currentBlob_ = Blob();
    this->currentBag_ = -1;
    this->indexBytes_.clear();
    this->source_.close();
    this->context_ = 0;
    this->open_ = false;
  }
} // namespace scrapbook
