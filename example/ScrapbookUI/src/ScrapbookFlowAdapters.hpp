#ifndef LOKA_SCRAPBOOK_UI_FLOW_ADAPTERS_HPP
#define LOKA_SCRAPBOOK_UI_FLOW_ADAPTERS_HPP

#include "ScrapbookPackage.hpp"
#include "dsl/flow/Flow.hpp"

namespace scrapbook
{
  enum ScrapbookFlowErrorCode
  {
    SCRAPBOOK_FLOW_PACKAGE_REFUSED = 20604
  };

  struct LoadPageAdapter
  {
    typedef int In;
    typedef PagePresentation Out;

    explicit LoadPageAdapter(ScrapbookPackage *package)
        : package_(package)
    {
    }

    loka::dsl::StepRunStatus run(const In &page, Out &out, loka::dsl::FlowError &error) const
    {
      if (!this->package_ || !this->package_->preparePage(page, out))
      {
        error.code = SCRAPBOOK_FLOW_PACKAGE_REFUSED;
        return loka::dsl::FLOW_STEP_FAILED;
      }
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    ScrapbookPackage *package_;
  };
} // namespace scrapbook

#endif // LOKA_SCRAPBOOK_UI_FLOW_ADAPTERS_HPP
