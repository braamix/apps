/* One thread. citrus_lock.h's WLOCK/UNLOCK are the only users, and they
 * become nothing. */
#ifndef _PTHREAD_H_
#define _PTHREAD_H_

typedef char pthread_rwlock_t;
#define PTHREAD_RWLOCK_INITIALIZER 0

#endif
