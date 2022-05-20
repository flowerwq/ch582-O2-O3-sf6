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
	uint8_t flag_init;
	uint16_t page_start;
	uint16_t page_active;
	st_page_t *pages;
} st_ctx_t;

static st_page_t st_pages[ST_PAGE_MAX];
static st_ctx_t st_ctx;

static const char *st_page_status_str(uint8_t status){
	switch(status){
		case ST_PAGE_S_ERASED:
			return "ERASED";
		case ST_PAGE_S_ACTIVE:
			return "ACTIVE";
		case ST_PAGE_S_FULL:
			return "FULL";
		case ST_PAGE_S_UNAVAILABLE:
			return "UNAVAIABLE";
		default:
			return "UNKNOWN";
	}
}

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
	LOG_DEBUG(TAG, "erase page %d", idx);
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
	page = &st_pages[idx];
	if (EEPROM_WRITE(idx * ST_PAGE_SIZE, &status, 1)){
		LOG_ERROR(TAG, "EEPROM write err.");
		return -1;
	}
	page->status = status;
	return 0;
}
static int st_page_erase_check(uint16_t idx){
	uint8_t buf[EEPROM_PAGE_SIZE];
	int i = 0;
	memset(buf, 0xff, EEPROM_PAGE_SIZE);
	uint32_t bytes_remain = ST_PAGE_SIZE;
	uint32_t bytes_verify = 0;
	uint32_t addr = idx * ST_PAGE_SIZE;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	while(bytes_remain){
		bytes_verify = MIN(EEPROM_PAGE_SIZE, bytes_remain);
		if (EEPROM_READ(addr, buf, bytes_verify)){
			LOG_ERROR(TAG, "read err.");
			goto fail;
		}
		for (i = 0; i < bytes_verify; i++){
			if (buf[i] != 0xff){
				return 0;
			}
		}
		bytes_remain -= bytes_verify;
	}
	return 1;
	
fail:
	return -1;
}
static int st_page_status(uint16_t idx){
	st_page_t *page = NULL;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	page = &st_pages[idx];
	return page->status;
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
static int st_page_last(uint16_t idx){
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (ST_PAGE_VALID(idx - 1)){
		return idx - 1;
	}
	return ST_PAGE_MAX - 1;
}

static int st_item_verify(st_item_t *item){
	crc_type_t crc_type = CRC8_MAXIM_INIT;
	uint32_t crc_value = 0;
	if (!item){
		return 0;
	}
	crc_value = crc_check(&crc_type, (const uint8_t *)item->content, 
		item->header.len);
	return (crc_value == item->header.crc);
}

static int st_page_write_item(uint16_t idx, st_item_t *item){
	st_page_t *page = NULL;
	uint8_t page_status = 0;
	uint16_t offset = 0;
	int bytes_write = 0;
	int ret = 0;
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	if (!ST_ITEM_IDX_VALID(item->header.idx)){
		LOG_ERROR(TAG, "invalid item idx.");
		return -1;
	}
	page = &st_pages[idx];
	switch(page->status){
		case ST_PAGE_S_FULL:
		case ST_PAGE_S_UNAVAILABLE:
			goto fail;
		case ST_PAGE_S_ERASED:
		case ST_PAGE_S_ACTIVE:
			break;
		default:
			LOG_ERROR(TAG, "invalid page status.");
			goto fail;
	}
	LOG_DEBUG(TAG, "write item(idx:%d,len:%d,crc:%d) to page %d, offset:%d", 
		item->header.idx, item->header.len, item->header.crc, idx, 
		page->bytes_used);
	bytes_write = item->header.len + sizeof(st_item_header_t);
	ret = st_page_write(idx, page->bytes_used, (uint8_t *)item, bytes_write);
	if (ret < 0){
		LOG_ERROR(TAG, "fail to write item.");
		goto fail;
	}
	page->bytes_used += bytes_write;
	page_status = page->status;
	if (ST_PAGE_S_ERASED == page->status){
		LOG_DEBUG(TAG, "page status change(%s -> %s)", 
			st_page_status_str(ST_PAGE_S_ERASED), 
			st_page_status_str(ST_PAGE_S_ACTIVE));
		page_status = ST_PAGE_S_ACTIVE;
	}
	if (ST_PAGE_CONTENT_MAX - page->bytes_used < sizeof(st_item_header_t)){
		LOG_DEBUG(TAG, "page status change(%s -> %s)", 
			st_page_status_str(ST_PAGE_S_ACTIVE), 
			st_page_status_str(ST_PAGE_S_FULL));
		page_status = ST_PAGE_S_FULL;
	}
	if (page_status != page->status){
		ret = st_page_status_update(idx, page_status);
		if (ret < 0){
			LOG_ERROR(TAG, "fail to update page status.");
			goto fail;
		}
	}
	page->item_cnt ++;
	return 0;
fail:
	return -1;
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
	page = &st_pages[idx];
	if (ST_PAGE_S_ERASED == page->status || 
		ST_PAGE_S_UNAVAILABLE == page->status )
	{
		return 0;
	}
	while(offset < page->bytes_used){
		ret = st_page_read(idx, offset, (uint8_t *)&item.header, 
			sizeof(st_item_header_t));
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
		LOG_DEBUG(TAG, "delete item(idx:%d) from page %d, offset:%d", 
			item.header.idx, idx, offset);
		item.header.idx = 0;
		ret = st_page_write(idx, offset, (uint8_t *)&item.header, 
			sizeof(st_item_header_t));
		if (ret < 0){
			LOG_ERROR(TAG, "fail to write item header.");
			goto fail;
		}
		if (page->item_cnt){
			page->item_cnt -= 1;
		}
		cnt ++;
		offset += item.header.len + sizeof(st_item_header_t);
	}
	if (!page->item_cnt && ST_PAGE_S_FULL == page->status){
		LOG_DEBUG(TAG, "erase page %d", idx);
		ret = st_page_erase(idx);
		if (ret < 0){
			LOG_ERROR(TAG, "fail to write item header.");
		}
	}
	return cnt;
fail:
	return -1;
}
int st_delete_item_with_loc(uint16_t item_idx, st_item_location_t *location){
	st_page_t *page = NULL;
	st_item_header_t header = {0};
	int ret = 0;
	if (!location){
		LOG_ERROR(TAG, "%s:param err", __FUNCTION__);
		goto fail;
	}
	if (!ST_PAGE_VALID(location->page)){
		goto fail;
	}
	page = &st_pages[location->page];
	if (!page->item_cnt){
		LOG_ERROR(TAG, "%s:target page is empty", __FUNCTION__);
		goto fail;
	}
	ret = st_page_read(location->page, location->offset, (uint8_t *)&header, 
		sizeof(st_item_header_t));
	if (ret < 0){
		LOG_ERROR(TAG, "%s:fail to read header", __FUNCTION__);
		goto fail;
	}
	if (header.idx != item_idx){
		LOG_ERROR(TAG, "%s:idx not match", __FUNCTION__);
		goto fail;
	}
	LOG_DEBUG(TAG, "delete item(idx:%d) from page %d, offset:%d", 
			item_idx, location->page, location->offset);
	header.idx = 0;
	ret = st_page_write(location->page, location->offset, (uint8_t *)&header, 
		sizeof(st_item_header_t));
	if (ret < 0){
		LOG_ERROR(TAG, "%s:fail to write header", __FUNCTION__);
		goto fail;
	}
	page->item_cnt --;
	return 0;
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
	page = &st_pages[idx];
	if (ST_PAGE_S_ERASED == page->status || 
		ST_PAGE_S_UNAVAILABLE == page->status)
	{
		return 0;
	}
	while(offset < page->bytes_used){
		ret = st_page_read(idx, offset, (uint8_t *)&item.header, sizeof(st_item_header_t));
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
		LOG_DEBUG(TAG, "find idx %d in page %d with offset %d", item.header.idx, 
			idx, offset);
		ret = st_page_read(idx, offset + sizeof(st_item_header_t), 
			item.content, item.header.len);
		if (ret < 0){
			LOG_ERROR(TAG, "fail to read item content.");
			goto fail;
		}
		if (!st_item_verify(&item)){
			LOG_ERROR(TAG, "item verify failed.");
			item.header.idx = 0;
			st_page_write(idx, offset, (uint8_t *)&item.header, sizeof(st_item_header_t));
			offset += item.header.len + sizeof(st_item_header_t);
			continue;
		}
		if (result){
			memcpy(result, &item, sizeof(st_item_t));
		}
		//delete last record if exist;
		if (ST_PAGE_VALID(search_ctx->last_location.page)){
			ret = st_delete_item_with_loc(search_ctx->item_idx, 
					&search_ctx->last_location);
			if (ret < 0){
				LOG_ERROR(TAG, "fail to delete last record");
				goto fail;
			}
		}
		search_ctx->last_location.page = idx;
		search_ctx->last_location.offset = offset;
		memcpy(&search_ctx->last_header, (uint8_t *)&item.header, sizeof(st_item_header_t));
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
	page = &st_pages[idx];
	uint32_t addr = idx * ST_PAGE_SIZE;
	uint32_t bytes_read = sizeof(item.header);
	page->bytes_used = 0;
	page->item_cnt = 0;
	LOG_DEBUG(TAG, "scan page %d", idx);
	while(page->bytes_used < ST_PAGE_CONTENT_MAX){
		if (!st_page_read(idx, page->bytes_used, (uint8_t *)&item.header, sizeof(item.header))){
			LOG_ERROR(TAG, "fail to read item header");
			goto fail;
		}
		if (0xFFU == item.header.len){
			break;
		}
		LOG_DEBUG(TAG, "item len:%d, idx:0x%04x, crc:%02x", item.header.len,
			item.header.idx, item.header.crc);
		page->bytes_used += item.header.len + sizeof(st_item_header_t);
		if (item.header.idx){
			page->item_cnt ++;
		}
	}
	if (page->bytes_used >= ST_PAGE_CONTENT_MAX){
		st_page_status_update(idx, ST_PAGE_S_FULL);
	}
	LOG_DEBUG(TAG, "page %d scan finish, %d bytes used", idx, page->bytes_used);
	return 0;
fail:
	return -1;
}

static int st_page_init(st_ctx_t *ctx, uint16_t idx){
	st_page_t *page = NULL;
	int bytes_read = 0;
	int ret = 0;
	if (!ST_PAGE_VALID(idx)){
		LOG_ERROR(TAG, "invalid idx");
		goto fail;
	}
	page = &ctx->pages[idx];
	page->item_cnt = 0;
	page->bytes_used = 0;
	memset(page, 0, sizeof(st_page_t));
	bytes_read = sizeof(page->status);
	if (EEPROM_READ(idx * ST_PAGE_SIZE, &page->status, bytes_read)){
		LOG_ERROR(TAG, "fail to read page status");
		goto fail;
	}
	switch(page->status){
		case ST_PAGE_S_FULL:
		case ST_PAGE_S_UNAVAILABLE:
			break;
		case ST_PAGE_S_ERASED:
			ret = st_page_erase_check(idx);
			if (ret < 0){
				LOG_ERROR(TAG, "erase check err.");
				goto fail;
			}
			if (!ret){
				LOG_ERROR(TAG, "erace check failed. erase again");
				if (st_page_erase(idx) < 0){
					LOG_ERROR(TAG, "erase err");
					goto fail;
				}
			}
			break;
		case ST_PAGE_S_ACTIVE:
			if (st_page_scan(idx)){
				LOG_ERROR(TAG, "fail to load page");
				goto fail;
			}
			ctx->page_active = idx;
			break;
		default:
			LOG_ERROR(TAG, "unknown status(%02x), erase whole page", page->status);
			if (st_page_erase(idx) < 0){
				LOG_ERROR(TAG, "erase err");
				goto fail;
			};
			page->status = 0xff;
	}
	return 0;
fail:
	return -1;
}

static int st_first_page(st_ctx_t *ctx, uint16_t page_from, uint8_t status){
	uint16_t i = 0;
	int page_start = page_from;
	if (!ST_PAGE_VALID(page_start)){
		return -1;
	}
	for(i = page_start; st_page_next(i) != page_start; i = st_page_next(i)){
		if (ctx->pages[i].status == status){
			return i;
		}
	}
	return ST_PAGE_MAX;
}

int st_erase_all(st_ctx_t *ctx){
	int i = 0;
	int ret = 0;
	st_page_status_t page_status = {0};
	for(i = 0; i < ST_PAGE_MAX; i++){
		if (ST_PAGE_S_ERASED == ctx->pages[i].status){
			continue;
		}
		ret = st_page_erase(i);
		if (ret < 0){
			LOG_ERROR(TAG, "%s:fail to erase page %d", __FUNCTION__, i);
			goto fail;
		}
	}
	ctx->page_active = ST_PAGE_MAX;
	return 0;
fail:
	return -1;
}

int st_init(){
	uint16_t i = 0;
	int ret = 0;
	st_page_status_t page_status = {0};
	st_ctx.pages = st_pages;
	st_ctx.page_start = ST_PAGE_MAX;
	st_ctx.page_active = ST_PAGE_MAX;
	lasttime = worktime_get();
	for (i = 0; i < ST_PAGE_MAX; i++){
		st_page_init(&st_ctx, i);
	}
	st_ctx.flag_init = 1;
	
out:
	return 0;
fail:
	return -1;
}
static int st_is_init(){
	return st_ctx.flag_init;
}
static int st_find_item(uint16_t item_idx, st_item_t *result, 
	st_item_location_t *location)
{
	st_ctx_t *ctx = &st_ctx;
	st_search_ctx_t search_ctx = {0};
	uint16_t page_start = ctx->page_active;
	int i = 0;
	int ret, cnt;
	ret = cnt = 0;
	if (!ST_ITEM_IDX_VALID(item_idx)){
		goto fail;
	}
	if (!ST_PAGE_VALID(page_start)){
		page_start = 0;
	}
	search_ctx.item_idx = item_idx;
	search_ctx.last_location.page = ST_PAGE_MAX;
	for(i = page_start; st_page_last(i) != page_start; i = st_page_last(i))
	{
		if (ST_PAGE_S_ERASED == st_page_status(i)){
			break;
		}
		if (cnt){
			result = NULL;
		}
		ret = st_page_find_item(&search_ctx, i, result);
		if (ret < 0){
			goto fail;
		}
		cnt += ret;
		if (1 == cnt && location){
			memcpy(location, &search_ctx.last_location, sizeof(st_item_location_t));
		}
		if (cnt > 1){
			ret = st_delete_item_with_loc(item_idx, &search_ctx.last_location);
			if (ret < 0){
				LOG_ERROR(TAG, "fail to delete item");
				goto fail;
			}
		}
	}
	
	return cnt;
fail:
	return -1;
}

static int st_page_get_freesize(st_ctx_t *ctx, uint16_t idx){
	if (!ST_PAGE_VALID(idx)){
		return -1;
	}
	switch(ctx->pages[idx].status){
		case ST_PAGE_S_ERASED:
		case ST_PAGE_S_FULL:
		case ST_PAGE_S_UNAVAILABLE:
			break;
		case ST_PAGE_S_ACTIVE:
			return ST_PAGE_CONTENT_MAX - ctx->pages[idx].bytes_used;
		default:
			return -1;
	}
	return 0;
}
static int st_get_available_page(st_ctx_t *ctx, uint16_t data_len){
	int i = 0;
	int ret = 0;
	uint16_t page_idx = ST_PAGE_MAX;
	st_page_status_t page_status = {0};
	if (!ctx){
		return -1;
	}
	if (data_len + sizeof(st_item_header_t) > ST_MAX_CONTENT_LEN){
		return -1;
	}
	if (ST_PAGE_VALID(ctx->page_active)){
		page_idx = ctx->page_active;
	}else{
		page_idx = st_first_page(ctx, 0, ST_PAGE_S_ACTIVE);
	}
	if (!ST_PAGE_VALID(page_idx)){
		LOG_ERROR(TAG, "no active page found");
		return -1;
	}
	if (st_page_get_freesize(ctx, page_idx) >= 
		data_len + sizeof(st_item_header_t))
	{
		return page_idx;
	}
	ret = st_page_status_update(page_idx, ST_PAGE_S_FULL);
	if (ret < 0){
		LOG_ERROR(TAG, "fail to write new item");
		goto fail;
	}
	page_idx = st_first_page(ctx, page_idx, ST_PAGE_S_ERASED);
	if (!ST_PAGE_VALID(page_idx)){
		LOG_ERROR(TAG, "no enough space");
		goto fail;
	}
	return page_idx;
fail:
	return -1;
}
int st_write_item(uint16_t item_idx, uint8_t *buf, int len)
{
	st_item_t item = {0};
	st_item_location_t location = {0};
	crc_type_t crc_type = CRC8_MAXIM_INIT;
	uint16_t page_idx = ST_PAGE_MAX;
	st_page_t *page = NULL;
	st_ctx_t *ctx = &st_ctx;
	int i = 0;
	int ret = 0;
	if (!buf || len <= 0 || len > ST_MAX_CONTENT_LEN){
		return -1;
	}
	if (!ST_ITEM_IDX_VALID(item_idx)){
		return -1;
	}
	ret = st_find_item(item_idx, &item, &location);
	if (ret < 0){
		LOG_ERROR(TAG, "fail to find item");
		goto fail;
	}
	
	item.header.idx = item_idx;
	item.header.len = len;
	memcpy(item.content, buf, len);
	item.header.crc = crc_check(&crc_type, (const uint8_t *)item.content, len);
	page_idx = st_get_available_page(ctx, len);
	if (!ST_PAGE_VALID(page_idx)){
		LOG_ERROR(TAG, "no available page");
		goto fail;
	}
	page = &ctx->pages[page_idx];
	ret = st_page_write_item(page_idx, &item);
	if (ret < 0){
		LOG_ERROR(TAG, "fail to write new item");
		goto fail;
	}
	if (ST_PAGE_VALID(location.page)){
		ret = st_delete_item_with_loc(item_idx, &location);
		if (ret < 0){
			LOG_ERROR(TAG, "fail to delete old item");
			goto fail;
		}
	}
	return 0;
fail:
	return -1;
}

int st_read_item(uint16_t item_idx, uint8_t *buf, int len)
{
	st_item_t item = {0};
	st_page_t *page = NULL;
	int i = 0;
	int ret = 0;
	if (!st_is_init()){
		return -1;
	}
	if (!buf || len <= 0){
		return -1;
	}
	if (!ST_ITEM_IDX_VALID(item_idx)){
		return -1;
	}
	ret = st_find_item(item_idx, &item, NULL);
	if (ret < 0){
		goto fail;
	}else if (0 == ret){
		return 0;
	}
	memcpy(buf, item.content, MIN(len, item.header.len));
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


int st_run(){
	int i = 0;
	int ret = 0;
	st_ctx_t *ctx = &st_ctx;
	if (!ctx->flag_init){
		return 0;
	}
	for(i = 0; i < ST_PAGE_MAX; i++){
		if (ST_PAGE_S_UNAVAILABLE == ctx->pages[i].status){
			ret = st_page_erase(i);
			if (ret < 0){
				LOG_ERROR(TAG, "fail to erase page %d", i);
			}
		}
	}
	return 0;
}
