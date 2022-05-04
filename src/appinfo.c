#include "appinfo.h"
#include "version.h"
appinfo_t appinfo __attribute__((section(".appinfo"))) = {
	APP_MAGIC,
	0x01,
	0x00,
	CURRENT_VERSION(),
	__DATE__ __TIME__,
};

