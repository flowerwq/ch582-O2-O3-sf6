/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : 串口1收发演示
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#include "CH58x_common.h"
#include "worktime.h"
#include "storage.h"
#include "upgrade.h"
#include "oled.h"
#include "bmp.h"
/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */

int uuid_dump(){
	uint8_t uuid[10] = {0};
	int i = 0;
	GET_UNIQUE_ID(uuid);
	PRINT("deviceid:");
	for (i = 0; i < 8; i++){
		PRINT("%02x", uuid[i]);
	}
	PRINT("\r\n");
	return 0;
}
int reset_dump(){
	SYS_ResetStaTypeDef rst = SYS_GetLastResetSta();
	PRINT("rst(");
	switch(rst){
		case RST_STATUS_SW:
			PRINT("sw reset");
			break;
		case RST_STATUS_RPOR:
			PRINT("poweron");
			break;
		case RST_STATUS_WTR:
			PRINT("wdt");
			break;
		case RST_STATUS_MR:
			PRINT("manual reset");
			break;
		case RST_STATUS_LRM0:
			PRINT("software wakeup");
			break;
		case RST_STATUS_GPWSM:
			PRINT("shutdown wakeup");
			break;
		case RST_STATUS_LRM1:
			PRINT("wdt wakeup");
			break;
		case RST_STATUS_LRM2:
			PRINT("manual wakeup");
			break;
	}
	PRINT(")\r\n");
	return 0;
}
int main()
{
    uint8_t len;
	worktime_t worktime = 0;
    SetSysClock(CLK_SOURCE_PLL_60MHz);
	worktime_init();
	
    /* 配置串口1：先配置IO口模式，再配置串口 */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);      // RXD-配置上拉输入
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA); // TXD-配置推挽输出，注意先让IO口输出高电平
    UART1_DefInit();

	reset_dump();
	PRINT("app start ...\r\n");
	uuid_dump();
	st_init();
	upgrade_init();
	OLED_Init();
	OLED_ShowString(0, 0, "Hi, Susie!", 16, 1);
	OLED_Refresh();
	DelayMs(1000);
	OLED_ShowString(0, 16, "Let's hang out ", 16, 1);
	OLED_ShowString(0, 32, "when you free !", 16, 1);
	OLED_Refresh();
	DelayMs(1000);
	OLED_Clear();
	PRINT("main loop start ...\r\n");
    while(1){
		OLED_Refresh();
		upgrade_run();
		st_run();
		if (worktime_since(worktime) > 1000){
			worktime = worktime_get();
			if (worktime % 2){
				OLED_ShowPicture(32, 0, 64, 64, smail_64x64_2, 1);
			}else{
				OLED_ShowPicture(32, 0, 64, 64, smail_64x64_1, 1);
			}
		}
	}
}

///*********************************************************************
// * @fn      UART1_IRQHandler
// *
// * @brief   UART1中断函数
// *
// * @return  none
// */
//__INTERRUPT
//__HIGH_CODE
//void UART1_IRQHandler(void)
//{
//    volatile uint8_t i;
//
//    switch(UART1_GetITFlag())
//    {
//        case UART_II_LINE_STAT: // 线路状态错误
//        {
//            UART1_GetLinSTA();
//            break;
//        }
//
//        case UART_II_RECV_RDY: // 数据达到设置触发点
//            for(i = 0; i != trigB; i++)
//            {
//                RxBuff[i] = UART1_RecvByte();
//                UART1_SendByte(RxBuff[i]);
//            }
//            break;
//
//        case UART_II_RECV_TOUT: // 接收超时，暂时一帧数据接收完成
//            i = UART1_RecvString(RxBuff);
//            UART1_SendString(RxBuff, i);
//            break;
//
//        case UART_II_THR_EMPTY: // 发送缓存区空，可继续发送
//            break;
//
//        case UART_II_MODEM_CHG: // 只支持串口0
//            break;
//
//        default:
//            break;
//    }
//}