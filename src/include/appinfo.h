#ifndef __APPINFO_H__
#define __APPINFO_H__

#include "stdint.h"
typedef struct appinfo_s{
	uint16_t magic;
	uint16_t vid;
	uint16_t pid;
	uint32_t version;
	uint8_t buildtime[32];
}appinfo_t;


#define APP_MAGIC	0x3736
#define APPINFO_OFFSET	4
#endif
