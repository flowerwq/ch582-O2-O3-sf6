#include <stdio.h>
#include <string.h>

#include "modbus.h"
#include "CH58x_common.h"
#include "worktime.h"

#define MB_ADDR	1

#define MB_BAUDRATE	115200
#define MB_TIMER_INIT_VALUE	(GetSysClock() * 5 / MB_BAUDRATE)

static int8_t mb_timer_ref;
static uint8_t mb_flag_transparent;
static mb_slave_ctx_t mb_slave_ctx;
static uint32_t mb_ts_last = 0;
#define MB_TIMER_IS_RUNNING() (R8_TMR0_CTRL_MOD & RB_TMR_COUNT_EN)
#define MB_TIMER_STOP()	TMR0_Disable()
#define MB_TIMER_RESUME()	TMR0_TimerInit(MB_TIMER_INIT_VALUE)

static void modbus_timer_enable(){
	if (!MB_TIMER_IS_RUNNING()){
		TMR0_TimerInit(MB_TIMER_INIT_VALUE);
	}
	mb_timer_ref ++;
}

static void modbus_timer_disable(){
	mb_timer_ref --;
	if (mb_timer_ref <= 0){
		TMR0_Disable();
	}
}
static void modbus_slave_set_idle(){
	mb_slave_ctx.flag_frame_err = 0;
	mb_slave_ctx.tcnt = 0;
	mb_slave_ctx.req_len = 0;
	mb_slave_ctx.status = MODBUS_S_IDLE;
}

/*********************************************************************
 * @fn      TMR0_IRQHandler
 *
 * @brief   TMR0中断函数
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void) // TMR0 定时中断
{
	if(TMR0_GetITFlag(TMR0_3_IT_CYC_END)){
		TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
	}
	mb_slave_ctx.tcnt ++;
	switch(mb_slave_ctx.status){
		case MODBUS_S_INIT:
			if (mb_slave_ctx.tcnt >= 7){
				mb_slave_ctx.tcnt = 0;
				mb_slave_ctx.status = MODBUS_S_IDLE;
			}
			break;
		case MODBUS_S_IDLE:
		case MODBUS_S_RECV_FINISH:
			mb_slave_ctx.tcnt = 0;
			break;
		case MODBUS_S_RECV:
			if (mb_slave_ctx.tcnt >= 3){
				mb_slave_ctx.status = MODBUS_S_CTRL_AND_WAITING;
				break;
			}
			break;
		case MODBUS_S_CTRL_AND_WAITING:
			if (mb_slave_ctx.tcnt >= 7){
				if (mb_slave_ctx.flag_frame_err){
					modbus_slave_set_idle();
				}else{
					mb_slave_ctx.status = MODBUS_S_RECV_FINISH;
				}
				modbus_timer_disable();
			}
			break;
		default:
			break;
	}
}

//init timer
static void modbus_timer_init(){

    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END); // 开启中断
    PFIC_EnableIRQ(TMR0_IRQn);
	TMR0_TimerInit(MB_TIMER_INIT_VALUE);
	TMR0_Disable();
}

void slave_recv(uint8_t d){
	int framelen, stoped;
	stoped = 0;
	if (MB_TIMER_IS_RUNNING()){
		MB_TIMER_STOP();
		stoped = 1;
	}
	framelen = sizeof(mb_slave_ctx.req_buf);
	switch(mb_slave_ctx.status){
		case MODBUS_S_INIT:
		case MODBUS_S_IDLE:
			mb_slave_ctx.tcnt = 0;
			mb_slave_ctx.status = MODBUS_S_RECV;
			mb_slave_ctx.req_buf[mb_slave_ctx.req_len] = d;
			mb_slave_ctx.req_len ++;
			break;
		case MODBUS_S_RECV:
			mb_slave_ctx.tcnt = 0;
			if (mb_slave_ctx.req_len < framelen){
				mb_slave_ctx.req_buf[mb_slave_ctx.req_len] = d;
				mb_slave_ctx.req_len ++;
			}
			break;
		case MODBUS_S_CTRL_AND_WAITING:
			mb_slave_ctx.tcnt = 0;
			mb_slave_ctx.flag_frame_err = 1;
			break;
		default:
			break;
	}
out:
	if (stoped){
		MB_TIMER_RESUME();

	}
	if (1 == mb_slave_ctx.req_len){
		modbus_timer_enable();
	}
}

ModbusError register_callback( const ModbusSlave *status, 
	const ModbusRegisterCallbackArgs *args,
	ModbusRegisterCallbackResult *out)
{
	switch(args->type){
	case MODBUS_HOLDING_REGISTER:
	case MODBUS_INPUT_REGISTER:
		return modbus_reg_callback(modbusSlaveGetUserPointer(status), args, out);
		break;
	}
	return 0;
}
LIGHTMODBUS_WARN_UNUSED ModbusError modbusStaticAllocator(ModbusBuffer *buffer, uint16_t size, void *context)
{
    mb_slave_ctx_t *ctx = (mb_slave_ctx_t *)context;
    if (!size)
    {
        // Pretend we're freeing the buffer
        buffer->data = NULL;
        return MODBUS_OK;
    }
    else
    {
        if (size > 256)
        {
            // Requested size is too big, return allocation error
            buffer->data = NULL;
            return MODBUS_ERROR_ALLOC;
        }
        else
        {
            // Return a pointer to our buffer
            buffer->data = ctx->resp_buf;
            return MODBUS_OK;
        }
    }
}

int modbus_slave_init(mb_callback_t *callback){
	memset(&mb_slave_ctx, 0, sizeof(mb_slave_ctx_t));
	mb_slave_ctx.address = MB_ADDR;
	mb_slave_ctx.slave.registerCallback = register_callback;
	if(callback){
		memcpy(&mb_slave_ctx.callback, callback, sizeof(mb_callback_t));
	}
	ModbusErrorInfo err = modbusSlaveInit(&mb_slave_ctx.slave, 
		register_callback, NULL, modbusStaticAllocator, NULL, 0);
	if (!modbusIsOk(err)){
		return -1;
	}
	modbusSlaveSetUserPointer(&mb_slave_ctx.slave, &mb_slave_ctx);
	return 0;
}

/*********************************************************************
 * @fn      UART2_IRQHandler
 *
 * @brief   UART2中断函数
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void UART2_IRQHandler(void)
{
    volatile uint8_t d;

    switch(UART2_GetITFlag())
    {
        case UART_II_LINE_STAT: // 线路状态错误
        {
            UART2_GetLinSTA();
            break;
        }

        case UART_II_RECV_RDY: // 数据达到设置触发点
            while(R8_UART2_FCR)
            {
                slave_recv(UART2_RecvByte());
            }
            break;

        case UART_II_RECV_TOUT: // 接收超时，暂时一帧数据接收完成
            while(R8_UART2_FCR)
            {
                slave_recv(UART2_RecvByte());
            }
            break;

        case UART_II_THR_EMPTY: // 发送缓存区空，可继续发送
            break;

        case UART_II_MODEM_CHG: // 只支持串口0
            break;

        default:
            break;
    }
}

int modbus_uart_init(){
	GPIOA_ModeCfg(GPIO_Pin_6, GPIO_ModeIN_PU);
	GPIOA_ModeCfg(GPIO_Pin_7, GPIO_ModeOut_PP_20mA);
	UART2_DefInit();
	UART2_BaudRateCfg(MB_BAUDRATE);
	UART2_ByteTrigCfg(UART_1BYTE_TRIG);
	PFIC_EnableIRQ(UART2_IRQn);
	UART2_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
	return 0;
}
int modbus_uart_send(uint8_t *buf, int len){
	if (!buf || len <= 0){
		return -1;
	}
	UART2_SendString(buf, len);
}
void modbus_init(mb_callback_t *callback){
	mb_timer_ref = 0;
	mb_ts_last = 0;
	mb_flag_transparent = 0;
	modbus_regs_init();
	if (modbus_slave_init(callback) < 0){
		return;
	}
	modbus_timer_init();
	modbus_uart_init();
}

void modbus_deinit(){
	UART2_INTCfg(DISABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
	MB_TIMER_STOP();
	modbusSlaveDestroy(&mb_slave_ctx.slave);
	memset(&mb_slave_ctx, 0, sizeof(mb_slave_ctx_t));
}

uint32_t modbus_lasttime_recv(){
	return mb_ts_last;
}

void modbus_frame_check(){
	ModbusErrorInfo err;
	if (MODBUS_S_RECV_FINISH == mb_slave_ctx.status){
		mb_ts_last = worktime_get()/1000;
		if (!mb_slave_ctx.flag_frame_err){
			PRINT("modbus parse ...");
			err = modbusParseRequestRTU( &mb_slave_ctx.slave, 
				mb_slave_ctx.address, mb_slave_ctx.req_buf, mb_slave_ctx.req_len);
			if (MODBUS_OK != modbusGetErrorCode(err)){
				PRINT("modbus parse err(%02x)", modbusGetErrorCode(err));
			}
			if (MODBUS_ERROR_ADDRESS != modbusGetErrorCode(err)){
				if (modbusSlaveGetResponseLength(&mb_slave_ctx.slave) > 0){
					modbus_uart_send(modbusSlaveGetResponse(&mb_slave_ctx.slave), 
						modbusSlaveGetResponseLength(&mb_slave_ctx.slave));
				}
			}
		}else{
			PRINT("modbus frame err");
		}
		modbus_slave_set_idle();
	}
}

int modbus_is_receiving(){
	return MODBUS_S_RECV == mb_slave_ctx.status;
}
