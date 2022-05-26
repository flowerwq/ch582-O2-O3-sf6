/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : 串口1收发演示
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/
#include "stdio.h"
#include "CH58x_common.h"
#include "worktime.h"
#include "storage.h"
#include "configtool.h"
#include "upgrade.h"
#include "oled.h"
#include "bmp.h"
#include "display.h"
#include "version.h"
#include "utils.h"

#define TAG "main"

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
    uint32_t appversion;
	worktime_t worktime = 0;
	char buf[2 * DISPLAY_LINE_LEN + 1];
    SetSysClock(CLK_SOURCE_PLL_60MHz);
	worktime_init();
	
    /* 配置串口1：先配置IO口模式，再配置串口 */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);      // RXD-配置上拉输入
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA); // TXD-配置推挽输出，注意先让IO口输出高电平
    UART1_DefInit();

	reset_dump();
	LOG_INFO(TAG, "start ...");
	uuid_dump();
	
	OLED_Init();
	OLED_ShowPicture(32, 0, 64, 64, (uint8_t *)smail_64x64_1, 1);
	OLED_Refresh();
	
	display_init();
	cfg_init();
	upgrade_init();
	while(worktime_since(worktime) < 1000){
		__nop();
	}
	OLED_Clear();
	DISPLAY_PRINT("Boot:%s", CURRENT_VERSION_STR());
	if (upgrade_app_available()){
		appversion = upgrade_app_version();
		version_str(appversion, buf, DISPLAY_LINE_LEN);
		DISPLAY_PRINT("APP:%s", buf);
	}else{
		DISPLAY_PRINT("APP:none");
	}
	if (upgrade_backup_available()){
		appversion = upgrade_backup_version();
		version_str(appversion, buf, DISPLAY_LINE_LEN);
		DISPLAY_PRINT("BAK:%s", buf);
	}else{
		DISPLAY_PRINT("BAK:none");
	}
	LOG_INFO(TAG, "main loop start ...");
	
    while(1){
		OLED_Refresh();
		upgrade_run();
//		if (worktime_since(worktime) >= 1000){
//			worktime = worktime_get();
//			if ((worktime / 1000) % 2){
//				display_printline(DISPLAY_LAST_LINE, "ts:%u", (uint32_t)worktime);
//			}else{
//				display_printline(DISPLAY_LAST_LINE, "abcdef abcdedg");
//			}
//		}
	}
}


