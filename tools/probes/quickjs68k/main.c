#include <stdio.h>
#include "quickjs.h"
#ifdef LOKA_QUICKJS_68K_PROBE
#include "memory.h"
#else
#define probe_new_runtime JS_NewRuntime
static size_t probe_memory_report(const char *phase)
{
  printf("%s: [skip] Classic allocator measurement on host\n", phase);
  return 0;
}
#endif

int main(void)
{
  JSRuntime *runtime;
  JSContext *context;
  JSValue value;
  int32_t result = 0;
  int failed = 1;
  probe_memory_report("before runtime");
  runtime = probe_new_runtime();
  if (!runtime)
  {
    printf("runtime allocation failed\n");
    goto finished;
  }
  JS_SetMemoryLimit(runtime, 512 * 1024);
  JS_SetMaxStackSize(runtime, 32 * 1024);
  probe_memory_report("runtime");
  context = JS_NewContext(runtime);
  if (!context)
  {
    printf("context allocation failed\n");
    JS_FreeRuntime(runtime);
    goto finished;
  }
  probe_memory_report("context");
  value = JS_Eval(context, "1+1", 3, "probe", JS_EVAL_TYPE_GLOBAL);
  failed = JS_IsException(value) || JS_ToInt32(context, &result, value) < 0 || result != 2;
  printf("result=%ld status=%s\n", (long)result, failed ? "FAIL" : "PASS");
  probe_memory_report("evaluated");
  JS_FreeValue(context, value);
  JS_FreeContext(context);
  JS_FreeRuntime(runtime);
finished:
  if (probe_memory_report("released") != 0)
    failed = 1;
  printf("final=%s\n", failed ? "FAIL" : "PASS");
#ifdef LOKA_QUICKJS_68K_PROBE
  printf("Press Return to exit.\n");
  getchar();
#endif
  return failed;
}
