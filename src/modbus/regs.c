#include <stdio.h>
#include <string.h>
#include "modbus.h"

#include "utils.h"
#include "version.h"

uint16_t mb_ro_regs[MB_REG_ADDR_RO_MAX - MB_REG_ADDR_RO_BASE];
uint16_t mb_config_regs[MB_REG_ADDR_CONFIG_MAX - MB_REG_ADDR_CONFIG_BASE];


/**
 *@desc update register
 *
 */
void modbus_reg_update(mb_reg_addr_t addr, uint16_t value)
{
	if (addr >= MB_REG_ADDR_RO_BASE && addr < MB_REG_ADDR_RO_MAX){
		mb_ro_regs[addr - MB_REG_ADDR_RO_BASE] = value;
	}
	if (addr >= MB_REG_ADDR_CONFIG_BASE && addr < MB_REG_ADDR_CONFIG_MAX){
		mb_config_regs[addr - MB_REG_ADDR_CONFIG_BASE] = value;
	}
}

uint16_t modbus_reg_get(mb_reg_addr_t addr)
{
	if (addr >= MB_REG_ADDR_RO_BASE && addr < MB_REG_ADDR_RO_MAX){
		return mb_ro_regs[addr - MB_REG_ADDR_RO_BASE];
	}
	if (addr >= MB_REG_ADDR_CONFIG_BASE && addr < MB_REG_ADDR_CONFIG_MAX){
		return mb_config_regs[addr - MB_REG_ADDR_CONFIG_BASE];
	}
	return 0;
}

/*
 *@desc 获取modbus寄存器实际内存地址，仅用于读取数据，注意越界
 */
const uint8_t *modbus_reg_buf_addr(mb_reg_addr_t addr){
	if (addr >= MB_REG_ADDR_RO_BASE && addr < MB_REG_ADDR_RO_MAX){
		return &mb_ro_regs[addr - MB_REG_ADDR_RO_BASE];
	}
	if (addr >= MB_REG_ADDR_CONFIG_BASE && addr < MB_REG_ADDR_CONFIG_MAX){
		return &mb_config_regs[addr - MB_REG_ADDR_CONFIG_BASE];
	}
	return NULL;
}

static int8_t modbus_reg_write(mb_reg_addr_t addr, uint16_t value)
{
	if (addr >= MB_REG_ADDR_CONFIG_BASE && addr < MB_REG_ADDR_CONFIG_MAX){
		mb_config_regs[addr-MB_REG_ADDR_CONFIG_BASE] = value;
	}
	return 0;
}


static uint16_t modbus_reg_read(mb_reg_addr_t addr)
{
	if (addr >= MB_REG_ADDR_RO_BASE && addr < MB_REG_ADDR_RO_MAX){
		return mb_ro_regs[addr - MB_REG_ADDR_RO_BASE];
	}
	if (addr >= MB_REG_ADDR_CONFIG_BASE && addr < MB_REG_ADDR_CONFIG_MAX){
		return mb_config_regs[addr - MB_REG_ADDR_CONFIG_BASE];
	}
	return 0;
}

static uint16_t modbus_reg_w_check(uint16_t index){
	if ((index >= MB_REG_ADDR_CONFIG_BASE && index < MB_REG_ADDR_CONFIG_MAX))
	{
		return 1;
	}
	return 0;
}

static uint16_t modbus_reg_r_check(uint16_t index){
	if ((index >= MB_REG_ADDR_RO_BASE && index < MB_REG_ADDR_RO_MAX) ||
		(index >= MB_REG_ADDR_CONFIG_BASE && index < MB_REG_ADDR_CONFIG_MAX))
	{
		return 1;
	}
	return 0;
}

ModbusError modbus_reg_callback(void *ctx, 
	const ModbusRegisterCallbackArgs *args,
	ModbusRegisterCallbackResult *out)
{
	mb_slave_ctx_t *sctx = (mb_slave_ctx_t *)ctx;
	out->exceptionCode = MODBUS_EXCEP_NONE;
	switch(args->query){
	case MODBUS_REGQ_R_CHECK:
		if (MODBUS_HOLDING_REGISTER == args->type){
			if(!modbus_reg_r_check(args->index)){
				return MODBUS_ERROR_ADDRESS;
			}
			return MODBUS_OK;
		}else{
			return MODBUS_ERROR_FUNCTION;
		}
		break;
	case MODBUS_REGQ_R:
		if (MODBUS_HOLDING_REGISTER == args->type){
			out->value = modbus_reg_read(args->index);
		}else{
			return MODBUS_ERROR_FUNCTION;
		}
		break;
	case MODBUS_REGQ_W_CHECK:
		if (MODBUS_HOLDING_REGISTER == args->type){
			return modbus_reg_w_check(args->index);
		}else{
			return MODBUS_ERROR_FUNCTION;
		}
		break;
	case MODBUS_REGQ_W:
		if (MODBUS_HOLDING_REGISTER == args->type){
			if (sctx && sctx->callback.before_reg_write)
			{
				if (sctx->callback.before_reg_write(args->index, args->value)){
					break;
				}
			}
			if (0 != modbus_reg_write(args->index, args->value)){
				break;
			}
			if (sctx && sctx->callback.after_reg_write){
				sctx->callback.after_reg_write(args->index, args->value);
			}
		}else{
			return MODBUS_ERROR_FUNCTION;
		}
		break;
	}
	return 0;
}

void modbus_reg_update_uid(uint8_t *uid, uint16_t len){
	uint8_t *buf = (uint8_t *)(mb_ro_regs + MB_REG_ADDR_UID_7);
	uint16_t maxlen = (MB_REG_ADDR_UID_0 - MB_REG_ADDR_UID_7 + 1) * 2;
	if (len < maxlen){
		buf += maxlen - len;
	}
	memcpy(buf, uid, MIN(len, maxlen));
}

void modbus_regs_init(){
	memset(mb_ro_regs, 0, sizeof(mb_ro_regs));
	memset(mb_config_regs, 0, sizeof(mb_config_regs));
	modbus_reg_update(MB_REG_ADDR_VERSION_H, CURRENT_VERSION() >> 16);
	modbus_reg_update(MB_REG_ADDR_VERSION_L, CURRENT_VERSION());
}

