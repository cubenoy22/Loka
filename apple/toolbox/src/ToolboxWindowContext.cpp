#include "ToolboxWindowContext.hpp"
#include "context/ToolboxNodeContextMapper.hpp"

ToolboxWindowContext::ToolboxWindowContext(int capabilities)
    : contextMapper_(new ToolboxNodeContextMapper(capabilities))
{
}

ToolboxWindowContext::~ToolboxWindowContext()
{
  delete contextMapper_;
  contextMapper_ = 0;
}
