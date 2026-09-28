/** Included by the production controller and Linux host fixture. */
void ToolboxScenePlatformController::updateStateFromEdit(EditTextControlBinding &binding)
{
  if (!binding.text || !binding.te)
  {
    return;
  }
  CharsHandle textHandle = TEGetText(binding.te);
  long length = 0;
  if (binding.te && *binding.te)
  {
    length = (**binding.te).teLength;
  }
  std::string utf8;
  if (textHandle && length > 0)
  {
    HLock(reinterpret_cast<Handle>(textHandle));
    const char *ptr = reinterpret_cast<const char *>(*textHandle);
    utf8.assign(ptr, static_cast<size_t>(length));
    HUnlock(reinterpret_cast<Handle>(textHandle));
  }
  // State notification fans out to every binding. Mark the typing source
  // current first so its sync is a no-op and preserves the active selection.
  binding.lastText = utf8;
  binding.textSeat.set(loka::core::String(utf8));
}

