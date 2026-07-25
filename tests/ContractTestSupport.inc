LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(LifecycleAuditedConstructorProbe)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(LifecycleAuditedConstructorProbeVerified)

namespace
{
  class LifecycleAuditedConstructorProbe LOKA_AUDITED(LifecycleAuditedConstructorProbe)
  {
  public:
    explicit LifecycleAuditedConstructorProbe(int value)
        : value_(value)
    {
    }

#ifdef LOKA_LIFECYCLE_AUDIT
    void lifecycleAuditReclassify(const char *tag, loka::core::LifecycleAuditDomain domain)
    {
      this->reclassifyLifecycleAudit(tag, domain);
    }
#endif

    int value() const
    {
      return value_;
    }

  private:
    int value_;
  };

  void testLifecycleAuditedCountsEveryConstructor()
  {
    LifecycleAuditedConstructorProbe original(1);
    LifecycleAuditedConstructorProbe copy(original);
    copy = original;
    assert(original.value() == 1);
    assert(copy.value() == 1);
    LOKA_AUDIT_RECLASSIFY_ALIVE(original, LifecycleAuditedConstructorProbeVerified,
                                loka::core::LIFECYCLE_AUDIT_CHAIN_RESIDENT);
    LOKA_AUDIT_RECLASSIFY_ALIVE(copy, LifecycleAuditedConstructorProbeVerified,
                                loka::core::LIFECYCLE_AUDIT_CHAIN_RESIDENT);
  }
} // namespace
