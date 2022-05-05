#include "appinfo.h"
#include "version.h"
appinfo_t appinfo __attribute__((section(".appinfo"))) = {
	APP_MAGIC,
	VID_CG,
	PID_BOOT,
	CURRENT_VERSION(),
	__DATE__ __TIME__,
};

