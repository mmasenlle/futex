#ifndef PERM_ID_T_H_
#define PERM_ID_T_H_

#include "linux/types.h"

typedef __u64 perm_ID_t;

#define perm_ID(_str) strtoull(_str, NULL, 10)

#endif /*PERM_ID_T_H_*/
