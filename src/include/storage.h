#ifndef __STORAGE_H__
#define __STORAGE_H__
#include "stdint.h"
#include "CH58x_common.h"

#define ST_PAGE_SIZE EEPROM_PAGE_SIZE
#define ST_PAGE_MAX	(EEPROM_MAX_SIZE/EEPROM_PAGE_SIZE)
#define ST_PAGE_VALID(p)	((p) >= 0 && (p) < ST_PAGE_MAX)

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

/**
 * The page is empty(all bytes are 0xff), Page is not used to store any data 
 * at this point.
 */
#define ST_PAGE_S_ERASED	0xFF
/**
 * Page is active, data can wirte here. No more than one page can be in this 
 * state at any given moment.
 */
#define ST_PAGE_S_ACTIVE		0xFE

/**
 *	The page is filled with key-value pairs, Writing new key-value pairs into 
 *	this page is not possible. It is still possible to mark some key-value 
 *	pairs as erased.
 */
#define ST_PAGE_S_FULL		0xFC

/**
 * Non-erased key-value pairs are being moved into another page so that the 
 * current page can be erased. 
 */
#define ST_PAGE_S_ERASEING	0xF8

/**
 * Page is full, and all data was marked as erased 
 */
#define ST_PAGE_S_UNAVAILABLE	0x00

#define ST_PAGE_HEADER_SIZE	sizeof(uint8_t)
#define ST_PAGE_CONTENT_MAX	(ST_PAGE_SIZE - ST_PAGE_HEADER_SIZE)

//#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
//struct st_page_s_parsed{
//	uint8_t flag_start:1;
//	uint8_t status:7;
//};
//#else
//struct st_page_s_parsed{
//	uint8_t status:7;
//	uint8_t flag_start:1;
//};
//
//#endif
//typedef union st_page_status{
//	struct st_page_s_parsed parsed;
//	uint8_t val;
//} st_page_status_t;

typedef uint8_t st_page_status_t;

#if ST_PAGE_SIZE <= 0xff
typedef struct st_page{
	uint8_t status;
	uint8_t bytes_used;
	uint8_t bytes_available;
} st_page_t;
#else
typedef struct st_page{
	uint8_t status;
	uint16_t item_cnt;
	uint16_t bytes_used;
	uint16_t bytes_available;
} st_page_t;
#endif

int st_init();
int st_read_item(uint16_t item_idx, uint8_t *buf, int len);
int st_write_item(uint16_t item_idx, uint8_t *buf, int len);


//int st_get_ota(st_ota_t *result);
//int st_update_ota(st_ota_t *val);

#endif
