#include "appinfo.h"

appinfo_t appinfo __attribute__((section(".appinfo"))) = {
	APP_MAGIC,
	0x01,
	0x01,
	0x01010011,
	__DATE__ __TIME__,
};

