#include <stdio.h>
#include <string.h>
#include "CH58x_common.h"
#include "storage.h"
#include "utils.h"
#include "worktime.h"

typedef struct st_item{
	int page;
	int offset;
	int len;
} st_item_t;
typedef enum st_item_idx{
	ST_IDX_OTA = 0,
	ST_IDX_MAX
}st_item_idx_t;

#define ST_IDX_VALID(idx)	((idx) >= ST_IDX_OTA && (idx) < ST_IDX_MAX)

uint16_t st_buf0[ST_P0_ADDR_MAX - ST_P0_ADDR_BASE];
//uint16_t st_buf1[ST_P1_ADDR_MAX - ST_P1_ADDR_BASE];
//uint16_t st_buf2[ST_P2_ADDR_MAX - ST_P2_ADDR_BASE];
//uint16_t st_buf3[ST_P3_ADDR_MAX - ST_P3_ADDR_BASE];
//uint16_t st_buf4[ST_P4_ADDR_MAX - ST_P4_ADDR_BASE];
//uint16_t st_buf5[ST_P5_ADDR_MAX - ST_P5_ADDR_BASE];
//uint16_t st_buf6[ST_P6_ADDR_MAX - ST_P6_ADDR_BASE];
//uint16_t st_buf7[ST_P7_ADDR_MAX - ST_P7_ADDR_BASE];

st_page_t st_pages[] = {
	{ST_P0_ADDR_BASE, ST_P0_ADDR_MAX - ST_P0_ADDR_BASE, 0,  st_buf0},
//	{ST_P1_ADDR_BASE, ST_P1_ADDR_MAX - ST_P1_ADDR_BASE, 0,  st_buf1},
//	{ST_P2_ADDR_BASE, ST_P2_ADDR_MAX - ST_P2_ADDR_BASE, 0,  st_buf2},
//	{ST_P3_ADDR_BASE, ST_P3_ADDR_MAX - ST_P3_ADDR_BASE, 0,  st_buf3},
//	{ST_P4_ADDR_BASE, ST_P4_ADDR_MAX - ST_P4_ADDR_BASE, 0,  st_buf4},
//	{ST_P5_ADDR_BASE, ST_P5_ADDR_MAX - ST_P5_ADDR_BASE, 0,  st_buf5},
//	{ST_P6_ADDR_BASE, ST_P6_ADDR_MAX - ST_P6_ADDR_BASE, 0,  st_buf6},
//	{ST_P7_ADDR_BASE, ST_P7_ADDR_MAX - ST_P7_ADDR_BASE, 0,  st_buf7},
};

static struct st_item items[] = {
	{0, ST_ADDR_OTA - ST_P0_ADDR_BASE, sizeof(st_ota_t)}
};


static void st_save(st_page_t *page){
	uint16_t i, offset;
	if (!page || page->len <= 0){
		return;
	}
	if (0 != EEPROM_ERASE(ST_ADDR_BASE + page->start, page->len)){
		return;
	}
	EEPROM_WRITE(ST_ADDR_BASE + page->start, page->content, page->len);
}
static void st_load(){
	int i;
	for (i = 0; i < sizeof(st_pages)/sizeof(st_page_t); i ++){
		EEPROM_READ(ST_ADDR_BASE + st_pages[i].start, 
			st_pages[i].content, st_pages[i].len);
		st_pages[i].checksum = crc16((uint8_t *)st_pages[i].content, 
			st_pages[i].len);
	}
}

static void st_check(){
	int i;
	uint16_t crc;
	for (i = 0; i < sizeof(st_pages)/sizeof(st_page_t); i ++){
		crc = crc16((uint8_t *)st_pages[i].content, st_pages[i].len);
		if (crc != st_pages[i].checksum){
			st_save(&st_pages[i]);
			st_pages[i].checksum = crc;
		}
	}
}

static worktime_t lasttime;
void st_init(){
	lasttime = worktime_get();
	st_load();
}
void st_run(){
	if (worktime_since(lasttime) >= 1000){
		lasttime = worktime_get();
		st_check();
	}
}

static int st_update_item(st_item_idx_t idx, uint8_t *buf, int len)
{
	if (!buf || len <= 0){
		return -1;
	}
	if (!ST_IDX_VALID(idx)){
		return -1;
	}
	st_item_t *item = &items[idx];
	st_page_t *page = &st_pages[item->page];
	memcpy(page->content + item->offset, buf, MIN(len, item->len));
	return 0;
}

static int st_read_item(st_item_idx_t idx, uint8_t *buf, int len)
{
	if (!buf || len <= 0){
		return -1;
	}
	if (!ST_IDX_VALID(idx)){
		return -1;
	}
	st_item_t *item = &items[idx];
	st_page_t *page = &st_pages[item->page];
	memcpy(buf, page->content + item->offset, MIN(len, item->len));
	return 0;
}

int st_get_ota(st_ota_t *result){
	int ret = 0;
	if (!result){
		return -1;
	}
	ret = st_read_item(ST_IDX_OTA, (uint8_t *)result, sizeof(st_ota_t));
	if (ret < 0){
		return -1;
	}
	return 0;
}

int st_update_ota(st_ota_t *val){
	int ret = 0;
	if (!val){
		return -1;
	}
	ret = st_update_item(ST_IDX_OTA, (uint8_t *)val, sizeof(st_ota_t));
	if (ret < 0){
		return -1;
	}
	return 0;
}

