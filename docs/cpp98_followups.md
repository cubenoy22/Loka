## C++98 follow-ups

1. **win32/src/SceneTests.hpp** still uses lambdas, `auto`, and `override`. Down-port or exclude it from legacy builds so the Win32 sample suite also compiles under C++98.
2. **NodeComposition DSL helpers** (`map`/`filter`) were removed for compliance. Re-introduce equivalents using explicit iterator adapters once a C++98-friendly design is ready, then wire them back into `FormScene`.
3. **Toolchain matrix**: run the full solution through the oldest supported MSVC/GCC toolchains and capture any additional incompatibilities (e.g., missing headers, template diagnostics) so we can bake them into CI.
4. **Docs/tests**: update README/build docs to mention the C++98 constraints and add regression tests (or scripts) that ensure accidental use of C++11 features is caught early.
5. **Cleanup staged work**: there were pre-existing staged edits unrelated to the C++98 retrofit. Decide whether to rebase or split them so future commits stay focused.
