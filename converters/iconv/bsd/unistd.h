/* Only issetugid is reached, and the port kit has no <unistd.h>. */
#ifndef _UNISTD_H_
#define _UNISTD_H_

#ifdef __cplusplus
extern "C" {
#endif

int issetugid(void);

#ifdef __cplusplus
}
#endif

#endif
