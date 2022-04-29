#ifndef __UTILS_H__
#define __UTILS_H__

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif

#define ABS(v)	((v) < 0 ? -(v) : (v))

#include "utils/crc16.h"
#include "utils/md5.h"
#endif

