#ifndef LOKA_TEST_SCRIPT_H
#define LOKA_TEST_SCRIPT_H

enum { smSysScript = 18, smRoman = 0 };
namespace toolbox_host
{
  extern long systemScript;
  extern unsigned scriptReads;
}
long GetScriptManagerVariable(short selector);

#endif
