#ifndef LOKA_SCRAPBOOK_UI_PACKAGE_HPP
#define LOKA_SCRAPBOOK_UI_PACKAGE_HPP

#include <cstddef>
#include <vector>

#include "ToolboxByteSource.hpp"
#include "app/PlatformContext.hpp"
#include "core/String.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "core/resource/lrpk/LrpkReader.hpp"

namespace scrapbook
{
  typedef loka::core::resource::lrpk::U32 AssetId;

#if defined(LOKA_SCRAPBOOK_PAGE_COUNT)
  const std::size_t kPageCount = LOKA_SCRAPBOOK_PAGE_COUNT;
#else
  const std::size_t kPageCount = 5;
#endif

  const AssetId kFirstPageAssetId = 1001UL;

#if defined(LOKA_SCRAPBOOK_ID_SPACE_STAMP)
  const AssetId kIdSpaceStamp = LOKA_SCRAPBOOK_ID_SPACE_STAMP;
#else
  const AssetId kIdSpaceStamp = 1585384077UL;
#endif

  inline AssetId PageAssetId(std::size_t page)
  {
    return kFirstPageAssetId + static_cast<AssetId>(page);
  }

  /** A page that has been fully read and decoded but not yet installed as the
      package owner's current page. It is a completed value so the node can
      publish it before asking the owner to retire the previous bag. */
  struct PagePresentation
  {
    PagePresentation()
        : bag(0),
          bagBlob(),
          image(),
          text(),
          caption(),
          badge(),
          isImage(false)
    {
    }

    std::size_t bag;
    loka::core::resource::Blob bagBlob;
    loka::core::resource::Image image;
    loka::core::String text;
    loka::core::String caption;
    loka::core::String badge;
    bool isImage;
  };

  /** Owns the file-backed package session: source, Reader borrow, index
      buffer, and the app-side one-bag Blob ledger live and retire together. */
  class ScrapbookPackage
  {
  public:
    ScrapbookPackage();
    ~ScrapbookPackage();

    bool open(PlatformContext *context);
    bool preparePage(int page, PagePresentation &out);
    void commitPage(const PagePresentation &page);
    void close();

    /** True once a page has been committed; the committed bag stays open
        through a refused prepare, so the shown page remains presentable. */
    bool hasCurrentPage() const;
    int currentPage() const;

  private:
    ScrapbookPackage(const ScrapbookPackage &);
    ScrapbookPackage &operator=(const ScrapbookPackage &);

    bool buildPresentation(int page, const loka::core::resource::Blob &blob, PagePresentation &out);
    void rollbackPreparedBag(std::size_t bag, bool openedNew);

    PlatformContext *context_;
    loka::toolbox::ToolboxByteSource source_;
    loka::core::resource::lrpk::Reader reader_;
    std::vector<unsigned char> indexBytes_;
    loka::core::resource::Blob currentBlob_;
    int currentBag_;
    bool open_;
  };
} // namespace scrapbook

#endif // LOKA_SCRAPBOOK_UI_PACKAGE_HPP
