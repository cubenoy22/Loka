#include "core/LifecycleAudit.hpp"

#ifdef LOKA_LIFECYCLE_AUDIT

#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace loka
{
  namespace core
  {
    namespace
    {
      enum
      {
        kMaxLifecycleAuditTags = 64
      };

      struct LifecycleAuditEntry
      {
        const char *tag;
        LifecycleAuditDomain domain;
        int alive;
      };

      LifecycleAuditEntry gLifecycleAuditEntries[kMaxLifecycleAuditTags];
      int gLifecycleAuditEntryCount = 0;

      const char *LifecycleAuditDomainName(LifecycleAuditDomain domain)
      {
        switch (domain)
        {
        case LIFECYCLE_AUDIT_CHAIN_RESIDENT:
          return "chain";
        case LIFECYCLE_AUDIT_PLATFORM_NATIVE:
          return "platform";
        case LIFECYCLE_AUDIT_PROCESS_GLOBAL:
          return "process-global";
        case LIFECYCLE_AUDIT_MIGRATION_INCOMPLETE:
          return "migration-incomplete";
        }
        return "unknown";
      }

      LifecycleAuditEntry *FindLifecycleAuditEntry(const char *tag)
      {
        for (int i = 0; i < gLifecycleAuditEntryCount; ++i)
        {
          if (std::strcmp(gLifecycleAuditEntries[i].tag, tag) == 0)
            return &gLifecycleAuditEntries[i];
        }
        return 0;
      }

      LifecycleAuditEntry *FindOrCreateLifecycleAuditEntry(const char *tag, LifecycleAuditDomain domain)
      {
        LifecycleAuditEntry *entry = FindLifecycleAuditEntry(tag);
        if (entry)
        {
          if (entry->domain != domain)
          {
            assert(false && "Lifecycle audit tag used in multiple domains");
            std::abort();
          }
          return entry;
        }

        if (gLifecycleAuditEntryCount >= kMaxLifecycleAuditTags)
        {
          assert(false && "Lifecycle audit registry overflow");
          std::abort();
        }
        entry = &gLifecycleAuditEntries[gLifecycleAuditEntryCount++];
        entry->tag = tag;
        entry->domain = domain;
        entry->alive = domain == LIFECYCLE_AUDIT_PROCESS_GLOBAL ? -1 : 0;
        return entry;
      }
    } // namespace

    void LifecycleAuditAliveIncrement(const char *tag, LifecycleAuditDomain domain)
    {
      LifecycleAuditEntry *entry = FindOrCreateLifecycleAuditEntry(tag, domain);
      if (entry->alive < 0)
      {
        if (entry->alive == INT_MIN)
        {
          assert(false && "Lifecycle audit process-global counter overflow");
          std::abort();
        }
        --entry->alive;
      }
      else
      {
        if (entry->alive == INT_MAX)
        {
          assert(false && "Lifecycle audit counter overflow");
          std::abort();
        }
        ++entry->alive;
      }
    }

    void LifecycleAuditAliveDecrement(const char *tag)
    {
      LifecycleAuditEntry *entry = FindLifecycleAuditEntry(tag);
      assert(entry && "Lifecycle audit decrement without increment");
      if (!entry)
        std::abort();

      if (entry->alive < 0)
      {
        if (entry->alive >= -1)
        {
          assert(false && "Lifecycle audit process-global counter underflow");
          std::abort();
        }
        ++entry->alive;
      }
      else
      {
        if (entry->alive <= 0)
        {
          assert(false && "Lifecycle audit counter underflow");
          std::abort();
        }
        --entry->alive;
      }
    }

    void LifecycleAuditReclassifyAlive(const char *fromTag, const char *toTag, LifecycleAuditDomain domain)
    {
      if (std::strcmp(fromTag, toTag) == 0)
      {
        LifecycleAuditEntry *entry = FindLifecycleAuditEntry(toTag);
        if (!entry || entry->domain != domain)
        {
          assert(false && "Lifecycle audit reclassification domain mismatch");
          std::abort();
        }
        return;
      }
      LifecycleAuditAliveDecrement(fromTag);
      LifecycleAuditAliveIncrement(toTag, domain);
    }

    void LifecycleAuditDeclareProcessGlobal(const char *tag)
    {
      LifecycleAuditEntry *entry = FindLifecycleAuditEntry(tag);
      if (!entry)
      {
        FindOrCreateLifecycleAuditEntry(tag, LIFECYCLE_AUDIT_PROCESS_GLOBAL);
        return;
      }
      if (entry->domain != LIFECYCLE_AUDIT_PROCESS_GLOBAL)
      {
        assert(false && "Lifecycle audit process-global tag already belongs to another domain");
        std::abort();
      }
    }

    void LifecycleAuditCheckpoint(const char *label)
    {
      bool failed = false;
      for (int i = 0; i < gLifecycleAuditEntryCount; ++i)
      {
        const LifecycleAuditEntry &entry = gLifecycleAuditEntries[i];
        if (entry.alive >= 0 && entry.alive != 0)
        {
          failed = true;
          break;
        }
      }
      if (!failed)
        return;

      std::fprintf(stderr, "[Loka lifecycle audit] checkpoint failed: %s\n", label ? label : "(unnamed)");
      std::fprintf(stderr, "domain\ttag\talive\n");
      for (int i = 0; i < gLifecycleAuditEntryCount; ++i)
      {
        const LifecycleAuditEntry &entry = gLifecycleAuditEntries[i];
        if (entry.alive != 0)
        {
          const int alive = entry.alive < 0 ? -entry.alive - 1 : entry.alive;
          std::fprintf(stderr, "%s\t%s\t%d%s\n",
                       LifecycleAuditDomainName(entry.domain), entry.tag, alive,
                       entry.alive < 0 ? " (process lifetime)" : "");
        }
      }
      std::abort();
    }
  } // namespace core
} // namespace loka

#endif // LOKA_LIFECYCLE_AUDIT
