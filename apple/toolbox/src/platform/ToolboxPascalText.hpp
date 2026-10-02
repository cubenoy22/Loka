#ifndef LOKA_TOOLBOX_PASCAL_TEXT_HPP
#define LOKA_TOOLBOX_PASCAL_TEXT_HPP

#include <Quickdraw.h>
#include "core/String.hpp"

/** Projects a short logical label to at most 255 native bytes.
    Reads the system script once: Roman uses System 7 MacRoman, others ASCII.
    Unmappable scalars and each malformed byte become '?'. Returns false only
    when UTF-8 collection fails, leaving out[0] zero; substitution/truncation
    are usable projections. Plain Text remains on its legacy path until T1b. */
bool ToolboxEncodePascal(const loka::core::String &value, Str255 out);

#endif
