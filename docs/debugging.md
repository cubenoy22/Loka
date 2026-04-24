# Debugging Notes

This document collects practical debugging notes that do not fit cleanly into build instructions or API documentation.

## Older Xcode on Leopard

On some Leopard-era Xcode debugger setups, breakpoints placed in `.hpp` code may not bind or stop reliably. This tends to show up in inline or template-heavy paths and can affect both PPC and Intel builds.

If a user or AI agent is trying to debug a code path on Leopard and ordinary header-file breakpoints do not stop, check this behavior before assuming the breakpoint location is wrong.

For temporary investigation, inserting `raise(SIGTRAP);` in the executed code path is a practical workaround.

- Use it only as a local debugging aid.
- Remove it after the investigation is complete.
- Include `<signal.h>` when needed.
