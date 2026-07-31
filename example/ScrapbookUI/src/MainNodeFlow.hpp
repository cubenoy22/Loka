#ifndef LOKA_SCRAPBOOK_UI_MAIN_NODE_FLOW_HPP
#define LOKA_SCRAPBOOK_UI_MAIN_NODE_FLOW_HPP

namespace scrapbook
{
  inline void MainNode::OnPageLoaded(const PagePresentation &page, void *userData)
  {
    MainNode *self = static_cast<MainNode *>(userData);
    if (self)
    {
      self->publishPage(page);
    }
  }

  inline loka::dsl::FlowHandleResult MainNode::OnPageLoadFailure(const loka::dsl::FlowError &, void *userData)
  {
    MainNode *self = static_cast<MainNode *>(userData);
    if (self)
    {
      if (self->package_.hasCurrentPage())
      {
        // preparePage rolled the refused bag back and the committed one is
        // still open, so the shown page is whole; put the selector back on it
        // rather than replacing good content with a refusal card.
        self->setRefusedPage(self->selectedPage_);
        self->selectedPage_ = self->package_.currentPage();
        self->page_.set(self->selectedPage_);
      }
      else
      {
        self->publishRefusal();
      }
    }
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  inline MainNode::PageFlowChain MainNode::buildFlow(MainNode &self)
  {
    PageFlowChain chain = loka::dsl::Flow()
                          | loka::dsl::Step(1, LoadPageAdapter(&self.package_))
                                .input(&self.selectedPage_)
                                .onSuccess(&MainNode::OnPageLoaded, &self);
    chain.onFailure(&MainNode::OnPageLoadFailure, &self);
    return chain;
  }
} // namespace scrapbook

#endif // LOKA_SCRAPBOOK_UI_MAIN_NODE_FLOW_HPP
