#ifndef __MODBUS_REGS_H__
#define __MODBUS_REGS_H__
#include "stdint.h"
#include "liblightmodbus.h"

#define MB_REG_CHANNEL_MAX 	32

#define MB_REG_APP_STATE_BOOT	1
#define MB_REG_APP_STATE_APP	2

#define MB_REG_DATA_BUF_SIZE	4096

typedef enum mb_reg_addr{
	MB_REG_ADDR_RO_BASE = 0,
	MB_REG_ADDR_APP_STATE = MB_REG_ADDR_RO_BASE,
	MB_REG_ADDR_VERSION_H,
	MB_REG_ADDR_VERSION_L,
	MB_REG_ADDR_UID_7,
	MB_REG_ADDR_UID_6,
	MB_REG_ADDR_UID_5,
	MB_REG_ADDR_UID_4,
	MB_REG_ADDR_UID_3,
	MB_REG_ADDR_UID_2,
	MB_REG_ADDR_UID_1,
	MB_REG_ADDR_UID_0,
	MB_REG_ADDR_APP_VID,
	MB_REG_ADDR_APP_PID,
	MB_REG_ADDR_APP_VERSION_H,
	MB_REG_ADDR_APP_VERSION_L,
	MB_REG_ADDR_BLOCK_SIZE,
	MB_REG_ADDR_BLOCK_NUM,
	MB_REG_ADDR_BUF_LEN_H,
	MB_REG_ADDR_BUF_LEN_L,
	MB_REG_ADDR_ENDIAN,
	MB_REG_ADDR_STANDBY,
	MB_REG_ADDR_OPT_CODE,
	MB_REG_ADDR_OPT_STATE,
	MB_REG_ADDR_OPT_PROGRESS,
	MB_REG_ADDR_OPT_ERR,
	MB_REG_ADDR_RO_MAX,
	
	MB_REG_ADDR_CONFIG_BASE = 128,
	MB_REG_ADDR_OPT_CTRL = MB_REG_ADDR_CONFIG_BASE,
	MB_REG_ADDR_DATA_LEN_H,
	MB_REG_ADDR_DATA_LEN_L,
	MB_REG_ADDR_DATA_CRC,
	MB_REG_ADDR_BUF_START,
	MB_REG_ADDR_BUF_MAX = MB_REG_ADDR_BUF_START + (MB_REG_DATA_BUF_SIZE/2),
	MB_REG_ADDR_CONFIG_MAX,
	
	MB_REG_ADDR_MAX
} mb_reg_addr_t;

void modbus_regs_init();
void modbus_reg_update(mb_reg_addr_t addr, uint16_t value);
uint16_t modbus_reg_get(mb_reg_addr_t addr);
void modbus_reg_update_uid(const uint8_t *uid, uint16_t len);
ModbusError modbus_reg_callback(void *ctx, 
	const ModbusRegisterCallbackArgs *args,
	ModbusRegisterCallbackResult *out);
const uint8_t *modbus_reg_buf_addr(mb_reg_addr_t addr);

#endif
