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

#define ST_PAGE_SIZE EEPROM_PAGE_SIZE
#define ST_PAGE_MAX	(EEPROM_MAX_SIZE/EEPROM_PAGE_SIZE)
#define ST_PAGE_VALID(p)	((p) >= 0 && (p) < ST_PAGE_MAX);

#define ST_ADDR_BASE	0x70000

#define ST_MAX_CONTENT_LEN	(EEPROM_PAGE_SIZE - 8)
#define ST_ITEM_IDX_MAX	0xffff
#define ST_ITEM_IDX_VALID(i)	((i) > 0 && (i) < ST_ITEM_IDX_MAX)
typedef struct st_item_header{
	uint8_t len;
	uint8_t crc;
	uint16_t idx;
}st_item_header_t;

typedef struct st_item{
	st_item_header_t header;
	uint8_t content[ST_MAX_CONTENT_LEN];
} st_item_t;

typedef enum st_addr{
	ST_P0_ADDR_BASE = 0,
	ST_ADDR_OTA = ST_P0_ADDR_BASE + 4,
	ST_ADDR_OTA_E = ST_ADDR_OTA + sizeof(st_ota_t),
	ST_P0_ADDR_MAX,
} st_addr_t;

#define ST_PAGE_S_ERASED	0x7F
#define ST_PAGE_S_INUSE		0x7E
#define ST_PAGE_S_FULL		0x7C
#define ST_PAGE_S_UNAVAILABLE	0x00

#define ST_PAGE_HEADER_SIZE	sizeof(uint8_t);
#define ST_PAGE_CONTENT_MAX	(ST_PAGE_SIZE - ST_PAGE_HEADER_SIZE)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
struct st_page_s_parsed{
	uint8_t flag_start:1;
	uint8_t status:7;
};
#else
struct st_page_s_parsed{
	uint8_t status:7;
	uint8_t flag_start:1;
};

#endif
typedef union st_page_status{
	struct st_page_s_parsed parsed;
	uint8_t val;
} st_page_status_t;

#if ST_PAGE_SIZE <= 0xff
typedef struct st_page{
	st_page_status_t status;
	uint8_t bytes_used;
	uint8_t bytes_available;
} st_page_t;
#else
typedef struct st_page{
	st_page_status_t status;
	uint16_t bytes_used;
	uint16_t bytes_available;
} st_page_t;
#endif

void st_run();
void st_init();

int st_get_ota(st_ota_t *result);
int st_update_ota(st_ota_t *val);

#endif
