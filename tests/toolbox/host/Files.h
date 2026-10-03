#ifndef LOKA_TEST_FILES_H
#define LOKA_TEST_FILES_H

#include <stdint.h>

typedef unsigned char Str63[64];
typedef unsigned char Str31[32];
typedef unsigned char Str255[256];
typedef int16_t OSErr;

// Classic alignment and field widths, independent of LP64 host long.
#pragma pack(push, 2)
struct FSSpec
{
  int16_t vRefNum;
  int32_t parID;
  Str63 name;
};
#pragma pack(pop)

enum { noErr = 0, fnfErr = -43, eofErr = -39, paramErr = -50,
       fsRdPerm = 1, fsFromStart = 1 };

// Production uses long out-parameters. Model their values, not the host ABI;
// the stored spec above must retain the Classic ABI.
OSErr FSpOpenDF(const FSSpec *, signed char, short *);
OSErr FSRead(short, long *, void *);
OSErr GetEOF(short, long *);
OSErr FSClose(short);
OSErr SetFPos(short, short, long);
OSErr FSMakeFSSpec(short, long, const unsigned char *, FSSpec *);
OSErr HGetVol(unsigned char *, short *, long *);
OSErr HSetVol(const unsigned char *, short, long);
OSErr FlushVol(const unsigned char *, short);

#endif
