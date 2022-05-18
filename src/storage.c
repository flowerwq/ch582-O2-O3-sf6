#include <stdio.h>
#include <string.h>
#include "CH58x_common.h"
#include "storage.h"
#include "utils.h"
#include "worktime.h"

#define TAG "ST"

typedef struct st_item_location{
	uint16_t page;
	uint16_t offset;
}st_item_location_t;

typedef struct st_search_context {
	uint16_t item_idx;
	st_item_header_t last_header;
	st_item_location_t last_location;
} st_search_ctx_t;

typedef struct st_context {
	uint16_t page_start;
	st_page_t *pages;
} st_ctx_t;

#define ST_IDX_VALID(idx)	((idx) >= ST_IDX_OTA && (idx) < ST_IDX_MAX)

uint16_t st_buf0[ST_P0_ADDR_MAX - ST_P0_ADDR_BASE];
//uint16_t st_buf1[ST_P1_ADDR_MAX - ST_P1_ADDR_BASE];
//uint16_t st_buf2[ST_P2_ADDR_MAX - ST_P2_ADDR_BASE];
//uint16_t st_buf3[ST_P3_ADDR_MAX - ST_P3_ADDR_BASE];
//uint16_t st_buf4[ST_P4_ADDR_MAX - ST_P4_ADDR_BASE];
//uint16_t st_buf5[ST_P5_ADDR_MAX - ST_P5_ADDR_BASE];
//uint16_t st_buf6[ST_P6_ADDR_MAX - ST_P6_ADDR_BASE];
//uint16_t st_buf7[ST_P7_ADDR_MAX - ST_P7_ADDR_BASE];

static struct st_item items[] = {
	{0, ST_ADDR_OTA - ST_P0_ADDR_BASE, sizeof(st_ota_t)}
};


static st_page_t st_pages[ST_PAGE_MAX];
static st_ctx_t st_ctx;
static int st_page_write(uint16_t idx, uint16_t offset, 
	uint8_t *buf, uint16_t len)
{
	uint32_t addr = idx * ST_PAGE_SIZE + ST_PAGE_HEADER_SIZE + offset;
	uint16_t bytes_remain = len;
	uint16_t bytes_write = 0;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (offset + len  + ST_PAGE_HEADER_SIZE >= ST_PAGE_SIZE){
		return -1;
	}
	while(bytes_remain){
		bytes_write = MIN(EEPROM_PAGE_SIZE, bytes_remain);
		if (EEPROM_WRITE(addr, buf, bytes_write)){
			LOG_ERROR(TAG, "EEPROM write err.");
			return -1;
		}
		addr += bytes_write;
		bytes_remain -= bytes_write;
	}
	return 0;
}

static int st_page_read(uint16_t idx, uint16_t offset, 
	uint8_t *buf, uint16_t len)
{
	uint32_t addr = idx * ST_PAGE_SIZE + ST_PAGE_HEADER_SIZE + offset;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (offset + len + ST_PAGE_HEADER_SIZE>= ST_PAGE_SIZE){
		return -1;
	}
	if (EEPROM_READ(addr, buf, len)){
		LOG_ERROR(TAG, "EEPROM write err.");
		return -1;
	}
	return 0;
}
static int st_page_erase(uint16_t idx){
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (EEPROM_ERASE(idx * ST_PAGE_SIZE, ST_PAGE_SIZE)){
		LOG_ERROR(TAG, "EEPROM erase err.");
		return -1;
	}
	return 0;
}
static int st_page_clear(uint16_t idx, uint16_t offset, 
	uint16_t len)
{
	uint8_t buf[EEPROM_PAGE_SIZE] = {0};
	uint32_t addr = idx * ST_PAGE_SIZE + ST_PAGE_HEADER_SIZE + offset;
	uint16_t bytes_remain = len;
	uint16_t bytes_write = 0;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (offset + len + ST_PAGE_HEADER_SIZE >= ST_PAGE_SIZE){
		return -1;
	}
	while(bytes_remain){
		bytes_write = MIN(bytes_remain, EEPROM_PAGE_SIZE);
		if (EEPROM_WRITE(addr, buf, bytes_write)){
			LOG_ERROR(TAG, "EEPROM write err.");
			return -1;
		}
		addr += bytes_write;
		bytes_remain -= bytes_write;
	}
	return 0;
}
static int st_page_status_update(uint16_t idx, uint8_t status){
	st_page_t *page = NULL;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	page = st_pages[idx];
	if (EEPROM_WRITE(idx * ST_PAGE_SIZE, &status, 1)){
		LOG_ERROR(TAG, "EEPROM write err.");
		return -1;
	}
	page->status = status;
	return 0;
}
static int st_page_status(uint16_t idx){
	st_page_t *page = NULL;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	page = &st_pages[idx];
	return page->status.parsed.status;
}

static int st_page_next(uint16_t idx){
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (ST_PAGE_VALID(idx + 1)){
		return idx + 1;
	}
	return 0;
}
static int st_item_verify(st_item_t *item){
	crc_type_t crc_type = CRC8_MAXIM_INIT;
	uint32_t crc_value = 0;
	if (!item){
		return 0;
	}
	crc_value = crc_check(crc_type, (const uint8_t *)item.content, 
		item->header.len);
	return (crc_value == item->header.crc);
}
static int st_page_delete_item(uint16_t idx, uint16_t item_idx)
{
	st_page_t *page = NULL;
	st_item_t item = {0};
	uint16_t offset = 0;
	int cnt = 0;
	int ret = 0;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (!ST_ITEM_IDX_VALID(item_idx)){
		LOG_ERROR(TAG, "invalid item idx.");
		return -1;
	}
	page = st_pages[idx];
	if (ST_PAGE_S_ERASED == page->status || ST_PAGE_S_UNAVAILABLE){
		return 0;
	}
	while(offset < page->bytes_used){
		ret = st_page_read(idx, offset, &item.header, sizeof(st_item_header_t));
		if (ret < 0){
			LOG_ERROR(TAG, "fail to read item header.");
			goto fail;
		}
		if (!item.header.idx){
			offset += item.header.len + sizeof(st_item_header_t);
			continue;
		}
		if (item.header.idx != item_idx){
			offset += item.header.len + sizeof(st_item_header_t);
			continue;
		}
		item.header.idx = 0;
		ret = st_page_write(idx, offset, &item.header, sizeof(st_item_header_t));
		if (ret < 0){
			LOG_ERROR(TAG, "fail to write item header.");
			goto fail;
		}
		cnt ++;
		offset += item.header.len + sizeof(st_item_header_t);
	}
	return cnt;
fail:
	return -1;
}

static int st_page_find_item(st_search_ctx_t *search_ctx, uint16_t idx, 
	st_item_t *result)
{
	st_page_t *page = NULL;
	st_item_t item = {0};
	uint16_t offset = 0;
	int cnt = 0;
	int ret = 0;
	if (!search_ctx){
		return -1;
	}
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (!ST_ITEM_IDX_VALID(search_ctx->item_idx)){
		LOG_ERROR(TAG, "invalid item idx.");
		return -1;
	}
	page = st_pages[idx];
	if (ST_PAGE_S_ERASED == page->status || ST_PAGE_S_UNAVAILABLE){
		return 0;
	}
	while(offset < page->bytes_used){
		ret = st_page_read(idx, offset, &item.header, sizeof(st_item_header_t));
		if (ret < 0){
			LOG_ERROR(TAG, "fail to read item header.");
			goto fail;
		}
		if (!item.header.crc || !item.header.idx){
			offset += item.header.len + sizeof(st_item_header_t);
			continue;
		}
		if (item.header.idx != search_ctx->item_idx){
			offset += item.header.len + sizeof(st_item_header_t);
			continue;
		}
		ret = st_page_read(idx, offset + sizeof(st_item_header_t), 
			item.content, item.header.len);
		if (ret < 0){
			LOG_ERROR(TAG, "fail to read item content.");
			goto fail;
		}
		if (!st_item_verify(&item)){
			LOG_ERROR(TAG, "item verify failed.");
			item.header.idx = 0;
			st_page_write(idx, offset, &item.header, sizeof(st_item_header_t));
			goto fail;
		}
		if (result){
			memcpy(result, &item, sizeof(st_item_t));
		}
		//delete last record if exist;
		if (ST_PAGE_VALID(search_ctx->last_location.page)){
			search_ctx->last_header.idx = 0;
			st_page_write(search_ctx->last_location.page, 
				search_ctx->last_location.offset, &search_ctx->last_header, 
				sizeof(st_item_header_t));
		}
		search_ctx->last_location.page = idx;
		search_ctx->last_location.offset = offset;
		memcpy(&search_ctx->last_header, &item.header, sizeof(st_item_header_t));
		cnt ++;
		offset += item.header.len + sizeof(st_item_header_t);
	}
	return cnt;
fail:
	return -1;
}

static worktime_t lasttime;

static int st_page_scan(uint16_t idx){
	st_page_t *page = NULL;
	st_item_t item = {0};
	page = st_pages[idx];
	uint32_t addr = idx * ST_PAGE_SIZE;
	uint32_t bytes_read = sizeof(item.header);
	page->bytes_used = 0;
	LOG_DEBUG(TAG, "scan page %d", idx);
	while(page->bytes_used < ST_PAGE_CONTENT_MAX){
		if (!st_page_read(idx, page->bytes_used, &item.header, sizeof(item.header))){
			LOG_ERROR(TAG, "fail to read item header");
			goto fail;
		}
		if (0xFFU == item.header.len){
			break;
		}
		LOG_DEBUG(TAG, "item len:%d, idx:0x%04x, crc:%02x", item.header.len,
			item.header.idx, item.header.crc);
		page->bytes_used += item.header.len;
	}
	if (page->bytes_used >= ST_PAGE_CONTENT_MAX){
		st_page_status_update(idx, ST_PAGE_S_FULL);
	}
	LOG_DEBUG(TAG, "page %d scan finish, %d bytes used", idx, page->bytes_used);
	return 0;
fail:
	return -1;
}

static int st_page_init(uint16_t idx){
	st_page_t *page = NULL;
	int bytes_read = 0;
	if (!ST_PAGE_VALID(idx)){
		LOG_ERROR(TAG, "invalid idx");
		goto fail;
	}
	page = st_pages[idx];
	memset(page, 0, sizeof(st_page_t));
	bytes_read = sizeof(page->status);
	if (!EEPROM_READ(idx * ST_PAGE_SIZE, &page->status, bytes_read)){
		LOG_ERROR(TAG, "fail to read page status");
		goto fail;
	}

	switch(page->status.parsed.status){
		case ST_PAGE_S_ERASED:
		case ST_PAGE_S_FULL:
		case ST_PAGE_S_UNAVAILABLE:
			return 0;
		case ST_PAGE_S_INUSE:
			if (st_page_scan(idx)){
				LOG_ERROR(TAG, "fail to load page");
				goto fail;
			}
		default:
			LOG_ERROR(TAG, "unknown page status(%02x)", page->status.parsed.status);
			goto fail;
	}
	return 0;
fail:
	return -1;
}


void st_init(){
	uint16_t i = 0;
	st_ctx.pages = st_pages;
	st_ctx.page_start = ST_PAGE_MAX;
	lasttime = worktime_get();
	for (i = 0; i < ST_PAGE_MAX; i++){
		st_page_init(i);
	}
}

static int st_update_item(uint16_t idx, uint8_t *buf, int len)
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
static int st_find_item(uint16_t item_idx, st_item_t *result){
	st_ctx_t *ctx = &st_ctx;
	st_search_ctx_t search_ctx = {0};
	int i = 0;
	int ret, cnt;
	ret = cnt = 0;
	if (!ST_ITEM_IDX_VALID(item_idx)){
		goto fail;
	}
	search_ctx.item_idx = item_idx;
	search_ctx.last_location.page = ST_PAGE_MAX;
	for(i = ctx->page_start; st_page_next(i) != ctx->page_start; 
		i = st_page_next(i))
	{
		if (ST_PAGE_S_ERASED == st_page_status(i)){
			break;
		}
		ret = st_page_find_item(&search_ctx, i, item_idx, result);
		if (ret < 0){
			goto fail;
		}
		cnt += ret;
	}
	return cnt;
fail:
	return -1;
}
int st_read_item(uint16_t item_idx, uint8_t *buf, int len)
{
	st_item_t item = {0};
	st_page_t *page = NULL;
	int i = 0;
	int ret = 0;
	if (!buf || len <= 0){
		return -1;
	}
	if (!ST_ITEM_IDX_VALID(item_idx)){
		return -1;
	}
	ret = st_find_item(item_idx, &item);
	if (ret < 0){
		goto fail;
	}else if (0 == ret){
		return 0;
	}
	memcpy(buf, item.content, MIN(len, item->header.len));
	return 1;
fail:
	return -1;
}

int st_delete_item(uint16_t item_idx){
	st_ctx_t *ctx = &st_ctx;
	int i = 0;
	int ret, cnt;
	ret = cnt = 0;
	if (!ST_ITEM_IDX_VALID(item_idx)){
		goto fail;
	}
	for(i = ctx->page_start; st_page_next(i) != ctx->page_start; 
		i = st_page_next(i))
	{
		if (ST_PAGE_S_ERASED == st_page_status(i)){
			break;
		}
		ret = st_page_delete_item(i, item_idx);
		if (ret < 0){
			goto fail;
		}
		cnt += ret;
	}
	return cnt;
fail:
	return -1;
}
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

