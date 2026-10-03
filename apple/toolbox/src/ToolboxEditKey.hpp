#ifndef LOKA_TOOLBOX_EDIT_KEY_HPP
#define LOKA_TOOLBOX_EDIT_KEY_HPP

/** Ordinary single-line input policy; TextEditor has its own command contract. */
enum ToolboxEditKey
{
  EDIT_KEY_NAVIGATE,
  EDIT_KEY_DELETE_BACKWARD,
  EDIT_KEY_DELETE_FORWARD,
  EDIT_KEY_INSERT,
  EDIT_KEY_REFUSE_HIGH,
  EDIT_KEY_CONSUME
};

inline ToolboxEditKey ClassifyEditKey(unsigned char key, bool romanInput)
{
  if (key >= 28 && key <= 31) return EDIT_KEY_NAVIGATE;
  if (key == 8) return EDIT_KEY_DELETE_BACKWARD;
  if (key == 0x7F) return EDIT_KEY_DELETE_FORWARD;
  if (key >= 0x80) return romanInput ? EDIT_KEY_INSERT : EDIT_KEY_REFUSE_HIGH;
  if (key >= 0x20) return EDIT_KEY_INSERT;
  return EDIT_KEY_CONSUME;
}

#endif
