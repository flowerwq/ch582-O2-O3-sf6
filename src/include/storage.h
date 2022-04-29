#ifndef __STORAGE_H__
#define __STORAGE_H__
#include "stdint.h"
typedef struct st_ota_s{
	uint32_t app_version;
	uint32_t app_size;
	uint8_t app_md5[16];
	uint32_t ota_version;
	uint32_t ota_size;
	uint8_t ota_md5[16];
}st_ota_t;

#define ST_ADDR_BASE	0x70000

typedef enum st_addr{
	ST_P0_ADDR_BASE = 0,
	ST_ADDR_OTA = ST_P0_ADDR_BASE,
	ST_ADDR_OTA_E = ST_ADDR_OTA + sizeof(st_ota_t),
	ST_P0_ADDR_MAX,
} st_addr_t;

typedef struct st_page{
	uint16_t start;
	uint16_t len;
	uint16_t checksum;
	uint16_t *content;
} st_page_t;
void st_run();
void st_init();

int st_get_ota(st_ota_t *result);
int st_update_ota(st_ota_t *val);

#endif
