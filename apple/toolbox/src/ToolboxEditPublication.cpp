/** Included by the production controller and Linux host fixture. */
void ToolboxScenePlatformController::updateStateFromEdit(
    EditTextControlBinding &binding, const loka::core::String &before)
{
  CharsHandle handle = TEGetText(binding.te);
  const long length = (**binding.te).teLength;
  loka::core::String decoded;
  bool accepted = false;
  if (handle && *handle && length >= 0)
  {
    const signed char state = HGetState(reinterpret_cast<Handle>(handle));
    HLock(reinterpret_cast<Handle>(handle));
    accepted = ToolboxDecodeNative(reinterpret_cast<const unsigned char *>(*handle),
                                  static_cast<std::size_t>(length), decoded);
    HSetState(reinterpret_cast<Handle>(handle), state);
  }
  if (!accepted)
  {
    binding.installed.revoke();
    ToolboxEditTextContext *context = static_cast<ToolboxEditTextContext *>(binding.ownerContext);
    context->invalidateNativePresentation();
    this->syncEditTextFromState(binding);
    this->addPendingDirty(context->chromeRect());
    SysBeep(1);
    return;
  }
  binding.installed.commit(decoded);
  if (decoded.equals(before)) return;
  // Mark before synchronous fanout; copy the seat and never touch the row after it.
  const loka::app::scene::WriteSeat<loka::core::String> seat = binding.textSeat;
  seat.set(decoded);
}
