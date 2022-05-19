#ifndef __CONFIG_TOOL_H__
#define __CONFIG_TOOL_H__

#include "stdint.h"

typedef struct cfg_ota_s{
	uint32_t app_version;
	uint32_t app_size;
	uint8_t app_md5[16];
	uint32_t ota_version;
	uint32_t ota_size;
	uint8_t ota_md5[16];
}cfg_ota_t;

typedef enum cfg_key{
	CFG_KEY_OTA = 1,
	CFG_KEY_MAX,
} cfg_key_t;

int cfg_init();

#endif
