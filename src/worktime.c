#include "CH58x_common.h"
#include "worktime.h"

static worktime_t worktime = 0;

__INTERRUPT
__HIGH_CODE
void SysTick_Handler(){
	worktime ++;
	SysTick->SR = 0;
}


int worktime_init(){
	SysTick_Config(GetSysClock()/1000);
}

worktime_t worktime_get(){
	return worktime;
}

worktime_t worktime_since(worktime_t from){
	return worktime - from;
}

