#include <stdio.h>
#include <string.h>
#include "stdint.h"
#include "CH58x_common.h"
#include "modbus.h"
#include "utils.h"
#include "worktime.h"
#include "uid.h"
#include "appinfo.h"
#include "storage.h"

typedef enum upgrade_status{
	UPGRADE_S_INIT = 0,
	UPGRADE_S_IDLE,
	UPGRADE_S_EXEC
}upgrade_status_t;
typedef enum upgrade_operation{
	UPGRADE_OPT_RESET = 0,
	UPGRADE_OPT_ERASE,
	UPGRADE_OPT_FLASH,
	UPGRADE_OPT_VERIFY,
}upgrade_opt_t;

typedef enum upgrade_operation_status{
	UPGRADE_OPT_S_NONE = 0,
	UPGRADE_OPT_S_EXEC,
	UPGRADE_OPT_S_FINISH,
	UPGRADE_OPT_S_FAIL
}upgrade_opt_status_t;

typedef enum upgrade_operation_err{
	UPGRADE_OPT_OK = 0,
	UPGRADE_OPT_ERR_ERASE,
	UPGRADE_OPT_ERR_INVALID_IMAGE,
	UPGRADE_OPT_ERR_VERIFY,
	UPGRADE_OPT_ERR_FLASH,
	UPGRADE_OPT_ERR_INVALID_LEN,
	UPGRADE_OPT_ERR_INVALID_OPT
}upgrade_opt_err_t;

typedef struct upgrade_flash_context{
	uint32_t addr;
	uint32_t len;
	uint32_t bytes_write;
	uint16_t data_crc;
	const uint8_t *data_buf;
	crc16_ctx_t flash_crc_ctx;
	uint16_t progress;
} upgrade_flash_ctx_t;

typedef struct upgrade_verify_context{
	uint32_t bytes_verified;
	uint16_t progress;
	uint8_t image_md5[16];
	MD5_CTX md5_ctx;
} upgrade_verify_ctx_t;


typedef struct upgrade_erase_context{
	uint32_t bytes_erased;
	uint16_t progress;
} upgrade_erase_ctx_t;

typedef union upgrade_opt_context{
	upgrade_flash_ctx_t flash;
	upgrade_erase_ctx_t erase;
	upgrade_verify_ctx_t verify;
} upgrade_opt_ctx_t;

typedef struct upgrade_context{
	st_ota_t otainfo;
	upgrade_status_t status;
	worktime_t lasttime;
	upgrade_opt_t opt_code;
	uint8_t flag_opt;
	uint8_t flag_standby;
	uint8_t app_available;
	appinfo_t appinfo;
	upgrade_opt_ctx_t opt_ctx;
	uint32_t image_size;
	uint32_t bytes_write;
	MD5_CTX md5_ctx;
} upgrade_ctx_t;

#define BOOT_PART_SIZE	(16 * 1024)
#define BOOT_BLOCK_NUM	(BOOT_PART_SIZE / EEPROM_BLOCK_SIZE)

#define APP_ADDR_START	BOOT_PART_SIZE
#define APP_PART_SIZE	(216 * 1024)
#define APP_ADDR_END	(APP_ADDR_START + APP_PART_SIZE)
#define APP_BLOCK_NUM	(APP_PART_SIZE / EEPROM_BLOCK_SIZE)

#define BACKUP_ADDR_START	APP_ADDR_END
#define BACKUP_PART_SIZE	(216 * 1024)
#define BACKUP_ADDR_END		(BACKUP_ADDR_START + BACKUP_PART_SIZE)
#define BACKUP_BLOCK_NUM	(BACKUP_PART_SIZE / EEPROM_BLOCK_SIZE)

#define APPINFO_ADDR	(APP_ADDR_START + APPINFO_OFFSET)
#define BACKUP_APPINFO_ADDR	(BACKUP_ADDR_START + APPINFO_OFFSET)
#define GOTO_AP()	((void (*)(void))APP_ADDR_START)()

static upgrade_ctx_t ctx;
static uint16_t upgrade_magic[4] = {0x4367U, 0x6366U, 0x426F, 0x6F74};
static uint8_t magic_cnt = 0;
int8_t mb_reg_after_write(mb_reg_addr_t addr, uint16_t value){
	switch(addr){
		case MB_REG_ADDR_OPT_CTRL:
			if (ctx.status == UPGRADE_S_INIT || ctx.status == UPGRADE_S_IDLE){
				ctx.flag_opt = 1;
			}
			break;
		default:
			break;
	}
	return 0;
}
int8_t mb_reg_before_write(mb_reg_addr_t addr, uint16_t value){
	if(UPGRADE_S_EXEC == ctx.status){
		return -1;
	}
	switch(addr){
		case MB_REG_ADDR_BUF_START:
			if (!ctx.flag_standby){
				if (magic_cnt < 0 || magic_cnt > 3){
					magic_cnt = 0;
					return -1;
				}
				if (value != upgrade_magic[magic_cnt]){
					magic_cnt = 0;
					return -1;
				}
				if (magic_cnt > 0 && modbus_reg_get(MB_REG_ADDR_BUF_START) != upgrade_magic[magic_cnt -1]){
					magic_cnt = 0;
					return -1;
				}
				magic_cnt ++;
				if (magic_cnt > 3){
					ctx.flag_standby = 1;
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

/**
 *	@brief verify app image
 *	@param appsize size of app image
 *	@param app_md5	md5 hash of app image
 *	@return 0 - success, (!0) - failure
 *
 */
static int upgrade_app_verify(int appsize, uint8_t *app_md5){
	MD5_CTX md5_ctx;
	int bytes_verified = 0;
	uint32_t bytes_read = 0;
	uint8_t buf[EEPROM_PAGE_SIZE];
	uint8_t md5[16] = {0};
	MD5Init(&md5_ctx);
	while(bytes_verified < appsize){
		bytes_read = min(EEPROM_BLOCK_SIZE, appsize - bytes_verified);
		FLASH_ROM_READ(APP_ADDR_START + bytes_verified, buf, bytes_read);
		MD5Update(&md5_ctx, buf, bytes_read);
	}
	MD5Final(md5, &md5_ctx);
	return memcmp(md5, app_md5, 16);
}

void upgrade_read_appinfo(){
	uint8_t buf[ALIGN_4(sizeof(appinfo_t))] = {0};
	appinfo_t *appinfo = (appinfo_t *)buf;
	st_ota_t otainfo = {0};
	st_get_ota(&otainfo);
	FLASH_ROM_READ(APPINFO_ADDR, buf, sizeof(appinfo_t));
	if (APP_MAGIC == appinfo->magic && 
		appinfo->version == otainfo.app_version)
	{
		if (0 == upgrade_app_verify(otainfo.app_size, otainfo.app_md5)){
			modbus_reg_update(MB_REG_ADDR_APP_VID, appinfo->vid);
			modbus_reg_update(MB_REG_ADDR_APP_PID, appinfo->pid);
			modbus_reg_update(MB_REG_ADDR_APP_VERSION_H, appinfo->version >> 16);
			modbus_reg_update(MB_REG_ADDR_APP_VERSION_L, appinfo->version);
			memcpy(&ctx.appinfo, buf, sizeof(appinfo_t));
			ctx.app_available = 1;
			return ;
		}
	}
	modbus_reg_update(MB_REG_ADDR_APP_VID, 0);
	modbus_reg_update(MB_REG_ADDR_APP_PID, 0);
	modbus_reg_update(MB_REG_ADDR_APP_VERSION_H, 0);
	modbus_reg_update(MB_REG_ADDR_APP_VERSION_L, 0);
	ctx.app_available = 0;
}
void upgrade_init(){
	uint32_t buflen;
	uint8_t uid[16] = {0};
	mb_callback_t mb_cb = {mb_reg_before_write, mb_reg_after_write, NULL, NULL};
	buflen = MB_REG_ADDR_BUF_MAX - MB_REG_ADDR_BUF_START;
	memset(&ctx, 0, sizeof(upgrade_ctx_t));
	ctx.lasttime = worktime_get();
	modbus_init(&mb_cb);
	modbus_reg_update(MB_REG_ADDR_BLOCK_NUM, APP_BLOCK_NUM);
	modbus_reg_update(MB_REG_ADDR_APP_STATE, MB_REG_APP_STATE_BOOT);
	
	upgrade_read_appinfo();
	
	modbus_reg_update(MB_REG_ADDR_BLOCK_SIZE, EEPROM_BLOCK_SIZE);
	modbus_reg_update(MB_REG_ADDR_BUF_LEN_H, buflen >> 16);
	modbus_reg_update(MB_REG_ADDR_BUF_LEN_L, buflen);
	modbus_reg_update_uid(get_uid(), UID_LENGTH);
}

void upgrade_opt_finish(upgrade_opt_err_t err){
	if (UPGRADE_OPT_OK != err){
		modbus_reg_update(MB_REG_ADDR_OPT_STATE, UPGRADE_OPT_S_FAIL);
		modbus_reg_update(MB_REG_ADDR_OPT_ERR, err);
	}else{
		modbus_reg_update(MB_REG_ADDR_OPT_STATE, UPGRADE_OPT_S_FINISH);
		modbus_reg_update(MB_REG_ADDR_OPT_ERR, UPGRADE_OPT_OK);
	}
	ctx.status = UPGRADE_S_IDLE;
}

void upgrade_flash_start(){
	modbus_reg_update(MB_REG_ADDR_OPT_CODE, UPGRADE_OPT_FLASH);
	modbus_reg_update(MB_REG_ADDR_OPT_STATE, UPGRADE_OPT_S_EXEC);
	ctx.opt_ctx.flash.len = modbus_reg_get(MB_REG_ADDR_DATA_LEN_H);
	ctx.opt_ctx.flash.len <<= 16;
	ctx.opt_ctx.flash.len += modbus_reg_get(MB_REG_ADDR_DATA_LEN_L);
	if (ctx.opt_ctx.flash.len <= 0){
		upgrade_opt_finish(UPGRADE_OPT_OK);
		return;
	}
	if(ctx.opt_ctx.flash.len > MB_REG_DATA_BUF_SIZE){
		upgrade_opt_finish(UPGRADE_OPT_ERR_INVALID_LEN);
		return;
	}
	ctx.opt_ctx.flash.addr = BACKUP_ADDR_START + ctx.bytes_write;
	ctx.opt_ctx.flash.data_crc = modbus_reg_get(MB_REG_ADDR_DATA_CRC);
	ctx.opt_ctx.flash.data_buf = modbus_reg_buf_addr(MB_REG_ADDR_BUF_START);
	if (crc16(ctx.opt_ctx.flash.data_buf, ctx.opt_ctx.flash.len) != 
		ctx.opt_ctx.flash.data_crc)
	{
		upgrade_opt_finish(UPGRADE_OPT_ERR_VERIFY);
		return;
	}
	if (!ctx.bytes_write){
		MD5Init(&ctx.md5_ctx);
	}
	ctx.opt_ctx.flash.bytes_write = 0;
	crc16_init(&ctx.opt_ctx.flash.flash_crc_ctx);
}

static int upgrade_is_image_valid(appinfo_t *img_info){
	if (APP_MAGIC != img_info->magic){
		goto fail;
	}
	if (ctx.app_available){
		if (ctx.appinfo.vid != img_info->vid || 
			ctx.appinfo.pid != img_info->pid)
		{
			goto fail;
		}
	}
	return 1;
fail:
	return 0;
}

void upgrade_flash_next(){
	uint16_t offset, crc_flash, progress;
	uint32_t addr, bytes_write;
	const uint8_t *idx;
	uint8_t buf[EEPROM_PAGE_SIZE] = {0};
	
	addr = ctx.opt_ctx.flash.addr + ctx.opt_ctx.flash.bytes_write;
	bytes_write = MIN(EEPROM_PAGE_SIZE, ctx.opt_ctx.flash.len);
	idx = ctx.opt_ctx.flash.data_buf + ctx.opt_ctx.flash.bytes_write;
	memcpy(buf, idx, bytes_write);
	
	//first packet
	if (!ctx.bytes_write){
		appinfo_t *appinfo = (appinfo_t *)(buf + APPINFO_OFFSET);
		if (!upgrade_is_image_valid(appinfo))
		{
			upgrade_opt_finish(UPGRADE_OPT_ERR_INVALID_IMAGE);
			return;
		}
	}
	if (FLASH_ROM_WRITE(addr, buf, bytes_write)){
		upgrade_opt_finish(UPGRADE_OPT_ERR_FLASH);
		return;
	}
	
	if (FLASH_ROM_VERIFY(addr, buf, bytes_write)){
		upgrade_opt_finish(UPGRADE_OPT_ERR_VERIFY);
		return;
	}
	
	progress = ctx.opt_ctx.flash.bytes_write * 100 / ctx.opt_ctx.flash.len;
	if (progress - ctx.opt_ctx.flash.progress >= 5){
		ctx.opt_ctx.flash.progress = progress;
		modbus_reg_update(MB_REG_ADDR_OPT_PROGRESS, progress);
	}
	
	crc16_update(&ctx.opt_ctx.flash.flash_crc_ctx, buf, bytes_write);
	ctx.opt_ctx.flash.bytes_write += bytes_write;
	
	if (ctx.opt_ctx.flash.bytes_write >= ctx.opt_ctx.flash.len){
		
		crc_flash = crc16_value(&ctx.opt_ctx.flash.flash_crc_ctx);;
		if (crc_flash != ctx.opt_ctx.flash.data_crc){
			upgrade_opt_finish(UPGRADE_OPT_ERR_VERIFY);
			return;
		}
		ctx.bytes_write += ctx.opt_ctx.flash.len;
		upgrade_opt_finish(UPGRADE_OPT_OK);
	}
}

int upgrade_erase_start(){
	int image_size = 0;
	ctx.opt_ctx.erase.bytes_erased = 0;
	ctx.opt_ctx.erase.progress = 0;
	modbus_reg_update(MB_REG_ADDR_OPT_CODE, UPGRADE_OPT_ERASE);
	modbus_reg_update(MB_REG_ADDR_OPT_STATE, UPGRADE_OPT_S_EXEC);
	modbus_reg_update(MB_REG_ADDR_OPT_PROGRESS, ctx.opt_ctx.erase.progress);
	image_size = modbus_reg_get(MB_REG_ADDR_DATA_LEN_H);
	image_size <<= 16;
	image_size += modbus_reg_get(MB_REG_ADDR_DATA_LEN_H);
	if (image_size > BACKUP_PART_SIZE){
		upgrade_opt_finish(UPGRADE_OPT_ERR_INVALID_LEN);
	}
	ctx.image_size = image_size;
	return 0;
}

int upgrade_verify_start(){
	memset(&ctx.opt_ctx.verify, 0, sizeof(upgrade_verify_ctx_t));
	const uint8_t data_buf = modbus_reg_buf_addr(MB_REG_ADDR_BUF_START);
	memcpy(ctx.opt_ctx.verify.image_md5, data_buf, 16);
	MD5Init(&ctx.opt_ctx.verify.md5_ctx);
	modbus_reg_update(MB_REG_ADDR_OPT_CODE, UPGRADE_OPT_VERIFY);
	modbus_reg_update(MB_REG_ADDR_OPT_STATE, UPGRADE_OPT_S_EXEC);
	modbus_reg_update(MB_REG_ADDR_OPT_PROGRESS, ctx.opt_ctx.verify.progress);
	return 0;
}

void upgrade_erase_next(){
	uint32_t addr;
	uint16_t progress;
	upgrade_erase_ctx_t *erase_ctx = &ctx.opt_ctx.erase;
	if (FLASH_ROM_ERASE(BACKUP_ADDR_START + erase_ctx->bytes_erased, 
		EEPROM_BLOCK_SIZE))
	{
		upgrade_opt_finish(UPGRADE_OPT_ERR_ERASE);
		return;
	}
	erase_ctx->bytes_erased += EEPROM_BLOCK_SIZE;
	if (erase_ctx->bytes_erased >= ctx.image_size){
		upgrade_opt_finish(UPGRADE_OPT_OK);
		return;
	}
	progress = erase_ctx->bytes_erased * 100 / ctx.image_size;
	if (progress - ctx.opt_ctx.erase.progress >= 5){
		ctx.opt_ctx.erase.progress = progress;
		modbus_reg_update(MB_REG_ADDR_OPT_PROGRESS, progress);
	}
}

static int upgrade_verify_next(){
	uint8_t buf[EEPROM_PAGE_SIZE] = {0};
	upgrade_verify_ctx_t *verify_ctx = &ctx.opt_ctx.verify;
	int bytes_remain = ctx.image_size - verify_ctx->bytes_verified;
	int bytes_read = MIN(EEPROM_PAGE_SIZE, bytes_remain);
	FLASH_ROM_READ(BACKUP_ADDR_START + verify_ctx->bytes_verified, 
		(void *)buf, bytes_read);
	MD5Update(&verify_ctx->md5_ctx, buf, bytes_read);
	verify_ctx->bytes_verified += bytes_read;
	if (verify_ctx->bytes_verified >= ctx.image_size){
		uint8_t md5[16] = {0};
		appinfo_t *appinfo = (appinfo_t *)buf;
		MD5Final(md5, &verify_ctx->md5_ctx);
		if (memcmp(md5, verify_ctx->image_md5, 16)){
			upgrade_opt_finish(UPGRADE_OPT_ERR_VERIFY);
			return -1;
		}
		FLASH_ROM_READ(BACKUP_APPINFO_ADDR, buf, sizeof(appinfo_t));
		ctx.otainfo.ota_size = ctx.image_size;
		ctx.otainfo.ota_version = appinfo->version;
		memcpy(ctx.otainfo.ota_md5, md5, 16);
		st_update_ota(&ctx.otainfo);
		upgrade_opt_finish(UPGRADE_OPT_OK);
		return 0;
	}
	return 0;
}

void upgrade_exec_next(){
	uint32_t irq = 0;
	SYS_DisableAllIrq(&irq);
	switch(ctx.opt_code){
		case UPGRADE_OPT_ERASE:
			upgrade_erase_next();
			break;
		case UPGRADE_OPT_FLASH:
			upgrade_flash_next();
			break;
		default:
			break;
	}
	SYS_RecoverIrq(irq);
}
void upgrade_reset(){
	modbus_deinit();
	PFIC_DisableAllIRQ();
	SYS_ResetExecute();
}
void upgrade_refresh(){
	upgrade_read_appinfo();
	upgrade_opt_finish(UPGRADE_OPT_OK);
}
void upgrade_exec_start(){
	uint16_t opt_code = modbus_reg_get(MB_REG_ADDR_OPT_CTRL);
	if (UPGRADE_S_EXEC == ctx.status){
		return;
	}
	ctx.status = UPGRADE_S_EXEC;
	ctx.opt_code = opt_code;
	modbus_reg_update(MB_REG_ADDR_OPT_CODE, opt_code);
	modbus_reg_update(MB_REG_ADDR_OPT_STATE, UPGRADE_OPT_S_EXEC);
	switch(opt_code){
		case UPGRADE_OPT_RESET:
			upgrade_reset();
			break;
		case UPGRADE_OPT_ERASE:
			upgrade_erase_start();
			break;
		case UPGRADE_OPT_FLASH:
			upgrade_flash_start();
			break;
		default:
			upgrade_opt_finish(UPGRADE_OPT_ERR_INVALID_OPT);
			break;
	}
	ctx.flag_opt = 0;
}

static int upgrade_copy_app(){
	uint8_t buf[EEPROM_PAGE_SIZE];
	uint32_t bytes_handled = 0;
	uint32_t bytes_read = 0;
	while(bytes_handled < ctx.otainfo.ota_size){
		FLASH_ROM_ERASE(APP_ADDR_START + bytes_handled, EEPROM_BLOCK_SIZE);
		bytes_handled += EEPROM_BLOCK_SIZE;
	}
	bytes_handled = 0;
	while(bytes_handled < ctx.otainfo.ota_size){
		bytes_read = MIN(EEPROM_PAGE_SIZE, ctx.otainfo.ota_size - bytes_handled);
		FLASH_ROM_READ(APP_ADDR_START + bytes_handled, buf, bytes_read);
		if (FLASH_ROM_WRITE(APP_ADDR_START + bytes_handled, buf, bytes_read)){
			goto fail;
		}
		if (FLASH_ROM_VERIFY(APP_ADDR_START + bytes_handled, buf, bytes_read)){
			goto fail;
		}
		bytes_handled += bytes_read;
	}
	ctx.otainfo.app_size = ctx.otainfo.ota_size;
	ctx.otainfo.app_version = ctx.otainfo.ota_version;
	memcpy(ctx.otainfo.app_md5, ctx.otainfo.ota_md5, 16);

	st_update_ota(&ctx.otainfo);
	return 0;
	
fail:
	return -1;
}
static void upgrade_jumpapp(){
	uint8_t need_copy = 0;
	PFIC_DisableAllIRQ();
	modbus_deinit();
	if (ctx.otainfo.ota_version != ctx.otainfo.app_version){
		upgrade_copy_app();
	}
	GOTO_AP();
}

void upgrade_run(){
	uint32_t irq = 0;
	modbus_frame_check();
	if (modbus_is_receiving()){
		return;
	}
	switch(ctx.status){
		case UPGRADE_S_INIT:
		case UPGRADE_S_IDLE:
			if (ctx.flag_opt){
				upgrade_exec_start();
			}else {
				if (ctx.app_available && (!ctx.flag_standby) && 
					worktime_since(ctx.lasttime) > 3000)
				{
					upgrade_jumpapp();
				}
			}
			break;
		case UPGRADE_S_EXEC:
			upgrade_exec_next();
			break;
		default:
			break;
	}
}