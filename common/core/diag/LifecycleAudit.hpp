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

    /**
     * Structurally records each audited instance. Copy construction adds a new
     * live instance; assignment preserves the existing instance identity.
     */
    template <typename Tag> class LifecycleAudited
    {
    protected:
      LifecycleAudited()
          : lifecycleAuditTag_(Tag::name())
      {
        LifecycleAuditAliveIncrement(this->lifecycleAuditTag_);
      }

      LifecycleAudited(const LifecycleAudited &)
          : lifecycleAuditTag_(Tag::name())
      {
        LifecycleAuditAliveIncrement(this->lifecycleAuditTag_);
      }

      ~LifecycleAudited()
      {
        LifecycleAuditAliveDecrement(this->lifecycleAuditTag_);
      }

      LifecycleAudited &operator=(const LifecycleAudited &)
      {
        return *this;
      }

      void reclassifyLifecycleAudit(const char *tag, LifecycleAuditDomain domain)
      {
        LifecycleAuditReclassifyAlive(this->lifecycleAuditTag_, tag, domain);
        this->lifecycleAuditTag_ = tag;
      }

    private:
      const char *lifecycleAuditTag_;
    };
  } // namespace core
} // namespace loka

// Audit tags are explicit because supported builds disable RTTI.
#define LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(Tag)                                                                         \
  namespace loka                                                                                                      \
  {                                                                                                                   \
    namespace core                                                                                                    \
    {                                                                                                                 \
      struct LifecycleAuditTag_##Tag                                                                                  \
      {                                                                                                               \
        static const char *name()                                                                                     \
        {                                                                                                             \
          return #Tag;                                                                                                \
        }                                                                                                             \
      };                                                                                                              \
    }                                                                                                                 \
  }

LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(Scene)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(Node)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(BoundaryNode)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(StateBase)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(NodeDefinitionBase)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(MenuBarDefinition)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(PushStateTracker)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(Window)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(SceneManager)

// Root and additional-base forms both vanish completely when auditing is off.
#define LOKA_AUDITED(Tag) : private ::loka::core::LifecycleAudited< ::loka::core::LifecycleAuditTag_##Tag >
#define LOKA_AUDITED_AS(Tag) , private ::loka::core::LifecycleAudited< ::loka::core::LifecycleAuditTag_##Tag >

/** Manual escape hatches for types that cannot use an inherited audit base. */
#define LOKA_AUDIT_ALIVE_INC(Tag) ::loka::core::LifecycleAuditAliveIncrement(#Tag)
#define LOKA_AUDIT_ALIVE_DEC(Tag) ::loka::core::LifecycleAuditAliveDecrement(#Tag)
#define LOKA_AUDIT_PROCESS_GLOBAL(Tag) ::loka::core::LifecycleAuditDeclareProcessGlobal(#Tag)
#define LOKA_AUDIT_RECLASSIFY_ALIVE(object, Tag, domain)                                                               \
  (object).lifecycleAuditReclassify(#Tag, (domain))
#define LOKA_AUDIT_CHECKPOINT(label) ::loka::core::LifecycleAuditCheckpoint(label)

#else

#define LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(Tag)
#define LOKA_AUDITED(Tag)
#define LOKA_AUDITED_AS(Tag)
#define LOKA_AUDIT_ALIVE_INC(Tag)
#define LOKA_AUDIT_ALIVE_DEC(Tag)
#define LOKA_AUDIT_PROCESS_GLOBAL(Tag)
#define LOKA_AUDIT_RECLASSIFY_ALIVE(object, Tag, domain)
#define LOKA_AUDIT_CHECKPOINT(label)

#endif

#endif // LOKA_CORE_LIFECYCLEAUDIT_HPP
