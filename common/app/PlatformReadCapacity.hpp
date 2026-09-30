#ifndef LOKA_APP_PLATFORMREADCAPACITY_HPP
#define LOKA_APP_PLATFORMREADCAPACITY_HPP

#include "app/PlatformContext.hpp"
#include "platform/file/FileIO.hpp"

namespace loka
{
  namespace app
  {
    /** Synchronous file-read admission using a borrowed platform context.
        Queries current contiguous allocation capacity on every check; a null
        context or an unanswered query permits the read. The context must
        outlive this policy and the read that borrows it. */
    class PlatformReadCapacity : public loka::platform::file::ReadCapacity
    {
    public:
      explicit PlatformReadCapacity(const PlatformContext *context)
          : context_(context)
      {
      }

      virtual bool allows(std::size_t requiredBytes) const
      {
        std::size_t largestAllocation = 0;
        return !this->context_ || !this->context_->queryLargestContiguousAllocation(largestAllocation) ||
               requiredBytes <= largestAllocation;
      }

    private:
      const PlatformContext *const context_;
    };
  } // namespace app
} // namespace loka

#endif // LOKA_APP_PLATFORMREADCAPACITY_HPP
