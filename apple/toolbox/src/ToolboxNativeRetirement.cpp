/** Shared deferred native queue and pool intake, included by the controller and host fixture. */
template <typename HandleT>
void ToolboxScenePlatformController::queueRetiredNativeHandle(std::vector<RetiredNativeEntry<HandleT> > &retired,
                                                              HandleT handle,
                                                              loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  if (!handle)
  {
    return;
  }
  for (size_t i = 0; i < retired.size(); ++i)
  {
    if (retired[i].handle == handle)
    {
      return;
    }
  }
  RetiredNativeEntry<HandleT> entry;
  entry.handle = handle;
  entry.lifetimeHint = lifetimeHint;
  retired.push_back(entry);
}

void ToolboxScenePlatformController::queueRetiredTextEdit(TEHandle te,
                                                          loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  queueRetiredNativeHandle(retiredTextEdits_, te, lifetimeHint);
}

bool ToolboxScenePlatformController::hasLiveBinding(TEHandle te) const
{
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].te == te)
    {
      return true;
    }
  }
  return false;
}

void ToolboxScenePlatformController::disposeNativeHandle(TEHandle te)
{
  if (te)
  {
    TEDispose(te);
  }
}

template <typename HandleT>
void ToolboxScenePlatformController::flushRetiredEntriesInto(
    std::vector<RetiredNativeEntry<HandleT> > &retired,
    loka::app::scene::ExactMatchHandleBucket<HandleT> &bucket)
{
  for (size_t i = 0; i < retired.size(); ++i)
  {
    HandleT handle = retired[i].handle;
    if (!handle)
    {
      continue;
    }
    if (retired[i].lifetimeHint == loka::app::scene::NATIVE_HINT_EAGER_RELEASE)
    {
      disposeNativeHandle(handle);
      continue;
    }
    // Bag entries must hold zero pointers into Loka; a live binding still
    // referencing the handle means the retire ritual did not complete.
    // Leaking the handle (counted) is the safe arm — disposing it would
    // hand the live binding a dead handle, and pooling it would pay the
    // same handle out twice.
    if (hasLiveBinding(handle))
    {
      ++poolIntakeAuditFailCount_;
      continue;
    }
    if (!bucket.offer(handle))
    {
      disposeNativeHandle(handle);
    }
  }
  retired.clear();
}

