#ifndef LOKA_TEST_SCRIPT_H
#define LOKA_TEST_SCRIPT_H

enum { smSysScript = 18, smKeyScript = 22, smRoman = 0, smSystemScript = -1 };
namespace toolbox_host
{
  extern long systemScript;
  extern long keyboardScript;
  extern unsigned scriptReads;
}
long GetScriptManagerVariable(short selector);

#endif
