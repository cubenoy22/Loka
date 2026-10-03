#ifndef LOKA_TOOLBOX_FILE_CHOICE_HPP
#define LOKA_TOOLBOX_FILE_CHOICE_HPP

#include <Files.h>
#include "core/io/File.hpp"

/** Capture a session-local native address snapshot, with display-only text.
    Invalid names or locator allocation refusal leave out unchanged. */
bool ToolboxCaptureChosenFile(const FSSpec &spec, loka::file::File &out);

/** Rebuild a zero-initialized spec from canonical bytes. Failure leaves out
    unchanged. Presence does not certify continued existence at that address. */
bool QueryToolboxSpec(const loka::file::File &file, FSSpec &out);

#endif
