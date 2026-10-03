#ifndef LOKA_TOOLBOX_EDIT_INSTALLED_HPP
#define LOKA_TOOLBOX_EDIT_INSTALLED_HPP

#include "core/String.hpp"
#include <TextEdit.h>

/** Completed ordinary TextEdit installation: present implies TE == Encode(source).
    None is distinct from a verified empty installation. Native bytes stay in TE. */
class ToolboxEditInstalled
{
public:
  ToolboxEditInstalled() : source_(), present_(false) {}
  bool holds(const loka::core::String &value) const
  { return this->present_ && this->source_.equals(value); }
  void commit(const loka::core::String &source)
  { this->source_ = source; this->present_ = true; }
  void revoke()
  { this->present_ = false; this->source_ = loka::core::String(); }
private:
  loka::core::String source_;
  bool present_;
};

/** Synchronous read-only paint input; never retained across callbacks or retirement. */
struct ToolboxEditPresentation
{
  const TEHandle te;
  const ToolboxEditInstalled installed;
  ToolboxEditPresentation() : te(0), installed() {}
  ToolboxEditPresentation(TEHandle handle, const ToolboxEditInstalled &certificate)
      : te(handle), installed(certificate) {}
};

#endif
