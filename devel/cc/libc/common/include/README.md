# Shared standard library headers

The C11 headers that do not depend on the target's data model, used by every target
after its own include directory (`libc/besm6/include`, `libc/riscv64/include`).  They get
their types from the target's `<stddef.h>`, `<stdarg.h>` and `<float.h>`.  See
[docs/Standard_Include_Files.md](../../../docs/Standard_Include_Files.md).
