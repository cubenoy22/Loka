#ifndef LOKA_CORE2_RUNTIME_ERROR_SINK_HPP
#define LOKA_CORE2_RUNTIME_ERROR_SINK_HPP

#include <string>
#include <vector>

namespace declara
{
  namespace core
  {
    namespace runtime
    {
      enum ErrorDomain
      {
        ERROR_DOMAIN_GENERIC = 0,
        ERROR_DOMAIN_IO = 1,
        ERROR_DOMAIN_BLOB = 2
      };

      struct ErrorEvent
      {
        ErrorDomain domain;
        int code;
        std::string message;
        std::string context;
        std::vector<std::string> tags;
        ErrorEvent() : domain(ERROR_DOMAIN_GENERIC), code(0), message(), context(), tags() {}
      };

      class ErrorSink
      {
      public:
        explicit ErrorSink(ErrorSink *parent = 0)
            : parent_(parent)
        {
        }

        void setParent(ErrorSink *parent) { parent_ = parent; }
        ErrorSink *parent() const { return parent_; }

        void addTag(const std::string &tag)
        {
          for (size_t i = 0; i < inheritedTags_.size(); ++i)
          {
            if (inheritedTags_[i] == tag)
              return;
          }
          inheritedTags_.push_back(tag);
        }

        void setTaskLabel(const std::string &label) { taskLabel_ = label; }

        void push(const ErrorEvent &event)
        {
          ErrorEvent enriched = event;
          if (!taskLabel_.empty())
            enriched.tags.push_back(taskLabel_);
          for (size_t i = 0; i < inheritedTags_.size(); ++i)
            enriched.tags.push_back(inheritedTags_[i]);
          events_.push_back(enriched);
          if (parent_)
            parent_->push(enriched);
        }

        const std::vector<ErrorEvent> &history() const { return events_; }
        void clearHistory() { events_.clear(); }

      private:
        std::vector<ErrorEvent> events_;
        ErrorSink *parent_;
        std::vector<std::string> inheritedTags_;
        std::string taskLabel_;
      };
    } // namespace runtime
  }   // namespace core
} // namespace declara

#endif // LOKA_CORE2_RUNTIME_ERROR_SINK_HPP
