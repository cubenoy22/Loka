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

  namespace
  {
    bool QueryPageResource(int page, const R::AssetRef *&out)
    {
      out = 0;
      if (page < 0 || static_cast<std::size_t>(page) >= R::Pages::AssetCount)
      {
        return false;
      }
      out = &R::Pages::Assets[page];
      return true;
    }
  } // namespace

  ScrapbookPackage::ScrapbookPackage()
      : context_(0),
        source_(),
        reader_(),
        indexBytes_(),
        uiBlob_(),
        refusedBadgeImage_(),
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
    if (!context->openFile(item, handle))
    {
      return false;
    }
#if defined(LOKA_RETRO68)
    if (!handle.hasSpec || !this->source_.open(handle.spec))
#else
    if (handle.displayPath.empty() || !this->source_.open(handle.displayPath))
#endif
    {
      return false;
    }

    std::size_t indexBytesNeeded = 0;
    if (this->reader_.beginOpen(this->source_, R::IdSpaceStamp, Reader::VERIFY_INTEGRITY, indexBytesNeeded)
        != Reader::OPEN_OK)
    {
      this->source_.close();
      return false;
    }

    this->indexBytes_.resize(indexBytesNeeded);
    unsigned char *indexBase = this->indexBytes_.empty() ? 0 : &this->indexBytes_[0];
    if (this->reader_.finishOpen(indexBase, this->indexBytes_.size()) != Reader::OPEN_OK
        || this->reader_.bagCount() != R::BagCount || this->reader_.assetCount() != R::AssetCount)
    {
      this->reader_.close();
      this->indexBytes_.clear();
      this->source_.close();
      return false;
    }

    this->context_ = context;
    this->open_ = true;
    // Package chrome is optional. A refused UI bag must not turn an otherwise
    // usable page package into a whole-package refusal.
    (void)this->loadRefusedBadge();
    return true;
  }

  bool ScrapbookPackage::loadRefusedBadge()
  {
    const R::AssetRef &resource = R::UI::RefusedBadge;
    std::size_t bagSize = 0;
    if (!this->reader_.bagStoredSize(resource.bag, bagSize))
    {
      return false;
    }

    Blob blob = Blob::Create();
    std::vector<unsigned char> &bytes = blob.mutableBytes();
    bytes.resize(bagSize);
    unsigned char *destination = bytes.empty() ? 0 : &bytes[0];
    if (this->reader_.readBagInto(resource.bag, destination, bytes.size()) != Reader::BAG_OK)
    {
      return false;
    }
    blob.sealBytes();

    Facts facts;
    Asset asset;
    Image image;
    if (this->reader_.get(resource.id, facts, asset) != Reader::GET_OK || asset.bag != resource.bag
        || asset.kind != resource.kind || resource.kind != loka::core::resource::lrpk::ASSET_KIND_IMAGE
        || !this->context_->createImageFromBlob(blob, asset.offsetInBag, asset.length, image))
    {
      this->reader_.closeBag(resource.bag);
      return false;
    }

    this->uiBlob_ = blob;
    this->refusedBadgeImage_ = image;
    return true;
  }

  bool ScrapbookPackage::preparePage(int page, PagePresentation &out)
  {
    out = PagePresentation();
    const R::AssetRef *resource = 0;
    if (!this->open_ || !QueryPageResource(page, resource))
    {
      return false;
    }

    const std::size_t bag = resource->bag;
    bool openedNew = false;
    Blob blob;
    if (static_cast<int>(bag) == this->currentBag_ && this->reader_.isBagOpen(bag))
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

    if (!this->buildPresentation(page, *resource, blob, out))
    {
      this->rollbackPreparedBag(bag, openedNew);
      return false;
    }
    return true;
  }

  bool ScrapbookPackage::buildPresentation(int page,
                                           const R::AssetRef &resource,
                                           const Blob &blob,
                                           PagePresentation &out)
  {
    Facts facts;
    Asset asset;
    if (this->reader_.get(resource.id, facts, asset) != Reader::GET_OK
        || asset.bag != resource.bag || asset.kind != resource.kind)
    {
      return false;
    }

    if (asset.kind != loka::core::resource::lrpk::ASSET_KIND_IMAGE
        && asset.kind != loka::core::resource::lrpk::ASSET_KIND_STRING)
    {
      return false;
    }
    const bool imagePage = asset.kind == loka::core::resource::lrpk::ASSET_KIND_IMAGE;

    PagePresentation next;
    next.page = page;
    next.bag = asset.bag;
    next.bagBlob = blob;
    next.caption = loka::core::String::FromInt(page + 1) + loka::core::String::Literal(" / ")
                   + loka::core::String::FromInt(static_cast<int>(kPageCount));
    next.badge = loka::core::String::Literal(imagePage ? "IMAGE" : "TEXT");
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

  bool ScrapbookPackage::hasCurrentPage() const
  {
    return this->currentPage() >= 0;
  }

  int ScrapbookPackage::currentPage() const
  {
    if (!this->open_ || this->currentBag_ < 0)
    {
      return -1;
    }
    for (std::size_t page = 0; page < kPageCount; ++page)
    {
      if (static_cast<int>(R::Pages::Assets[page].bag) == this->currentBag_)
      {
        return static_cast<int>(page);
      }
    }
    return -1;
  }

  Image ScrapbookPackage::refusedBadgeImage() const
  {
    return this->refusedBadgeImage_;
  }

  void ScrapbookPackage::rollbackPreparedBag(std::size_t bag, bool openedNew)
  {
    if (openedNew)
    {
      this->reader_.closeBag(bag);
    }
  }

  void ScrapbookPackage::releaseUiBag()
  {
    this->refusedBadgeImage_ = Image::Empty();
    this->uiBlob_ = Blob();
    if (this->reader_.isBagOpen(R::UI::RefusedBadge.bag))
    {
      this->reader_.closeBag(R::UI::RefusedBadge.bag);
    }
  }

  void ScrapbookPackage::close()
  {
    this->releaseUiBag();
    this->currentBlob_ = Blob();
    if (this->currentBag_ >= 0 && this->reader_.isBagOpen(static_cast<std::size_t>(this->currentBag_)))
    {
      this->reader_.closeBag(static_cast<std::size_t>(this->currentBag_));
    }
    this->reader_.close();
    this->currentBag_ = -1;
    this->indexBytes_.clear();
    this->source_.close();
    this->context_ = 0;
    this->open_ = false;
  }
} // namespace scrapbook
