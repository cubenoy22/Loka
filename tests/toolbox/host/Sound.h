#ifndef LOKA_HOST_SOUND_H
#define LOKA_HOST_SOUND_H
namespace toolbox_host { extern unsigned beeps; }
inline void SysBeep(short) { ++toolbox_host::beeps; }
#endif
