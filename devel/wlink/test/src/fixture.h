// What a fixture sees of the driver: output into one buffer, written once.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void fx_puts(const char *s);
void fx_putn(long n);
unsigned fx_pid(void);

// The fixture's own entry, called from proc_main after the constructors.
void fx_main(void);

#ifdef __cplusplus
}
#endif
