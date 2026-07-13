#ifndef LOKA_CORE_LIFECYCLEAUDIT_HPP
#define LOKA_CORE_LIFECYCLEAUDIT_HPP

#ifdef LOKA_LIFECYCLE_AUDIT

namespace loka
{
  namespace core
  {
    /** Lifecycle ownership domains used by the audit registry. */
    enum LifecycleAuditDomain
    {
      LIFECYCLE_AUDIT_CHAIN_RESIDENT = 0,
      LIFECYCLE_AUDIT_PLATFORM_NATIVE = 1,
      LIFECYCLE_AUDIT_PROCESS_GLOBAL = 2,
      LIFECYCLE_AUDIT_MIGRATION_INCOMPLETE = 3
    };

    /** Records construction and destruction against a stable type tag. */
    void LifecycleAuditAliveIncrement(const char *tag, LifecycleAuditDomain domain = LIFECYCLE_AUDIT_CHAIN_RESIDENT);
    void LifecycleAuditAliveDecrement(const char *tag);
    /** Moves one live object to an explicitly declared audit domain and tag. */
    void LifecycleAuditReclassifyAlive(const char *fromTag, const char *toTag, LifecycleAuditDomain domain);
    void LifecycleAuditDeclareProcessGlobal(const char *tag);
    /** Aborts after printing all nonzero tags when a checked domain is not empty. */
    void LifecycleAuditCheckpoint(const char *label);
  } // namespace core
} // namespace loka

#define LOKA_AUDIT_ALIVE_INC(Tag) ::loka::core::LifecycleAuditAliveIncrement(#Tag)
#define LOKA_AUDIT_ALIVE_DEC(Tag) ::loka::core::LifecycleAuditAliveDecrement(#Tag)
#define LOKA_AUDIT_PROCESS_GLOBAL(Tag) ::loka::core::LifecycleAuditDeclareProcessGlobal(#Tag)
#define LOKA_AUDIT_RECLASSIFY_ALIVE(object, Tag, domain)                                                               \
  (object).lifecycleAuditReclassify(#Tag, (domain))
#define LOKA_AUDIT_CHECKPOINT(label) ::loka::core::LifecycleAuditCheckpoint(label)

#else

#define LOKA_AUDIT_ALIVE_INC(Tag)
#define LOKA_AUDIT_ALIVE_DEC(Tag)
#define LOKA_AUDIT_PROCESS_GLOBAL(Tag)
#define LOKA_AUDIT_RECLASSIFY_ALIVE(object, Tag, domain)
#define LOKA_AUDIT_CHECKPOINT(label)

#endif

#endif // LOKA_CORE_LIFECYCLEAUDIT_HPP
