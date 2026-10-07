#ifndef LOKA_TOOLBOX_MULTIVERSAL_FILES_H
#define LOKA_TOOLBOX_MULTIVERSAL_FILES_H

#include <Multiverse.h>
#include <cstring>

// Universal Interfaces correctly describes FSpOpenDF's input specification
// as const. Preserve that contract at Loka call sites while adapting to
// Multiversal's older declaration.
inline OSErr FSpOpenDF(const FSSpec *spec, SignedByte permission, int16_t *refNum)
{
  return FSpOpenDF(const_cast<FSSpec *>(spec), permission, refNum);
}

// Match Universal Interfaces' read-only FSSpec input for the prepare door.
inline OSErr FSpGetFInfo(const FSSpec *spec, FInfo *info)
{
  return FSpGetFInfo(const_cast<FSSpec *>(spec), info);
}

inline OSErr FSpCreate(const FSSpec *spec, OSType creator, OSType type, ScriptCode script)
{
  return FSpCreate(const_cast<FSSpec *>(spec), creator, type, script);
}

inline OSErr HGetVol(StringPtr volumeName, short *vRefNum, long *dirId)
{
  WDPBRec params;
  std::memset(&params, 0, sizeof(params));
  params.ioNamePtr = volumeName;
  const OSErr err = PBHGetVolSync(&params);
  if (err == noErr)
  {
    *vRefNum = params.ioVRefNum;
    *dirId = params.ioWDDirID;
  }
  return err;
}

inline OSErr HSetVol(ConstStringPtr volumeName, short vRefNum, long dirId)
{
  WDPBRec params;
  std::memset(&params, 0, sizeof(params));
  params.ioNamePtr = const_cast<StringPtr>(volumeName);
  params.ioVRefNum = vRefNum;
  params.ioWDDirID = dirId;
  return PBHSetVolSync(&params);
}

#endif
