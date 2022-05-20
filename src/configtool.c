#include "configtool.h"
#include "storage.h"
#include "utils.h"

int cfg_get_ota(cfg_ota_t *result){
	int ret = 0;
	if (!result){
		return -1;
	}
	ret = st_read_item(CFG_KEY_OTA, (uint8_t *)result, sizeof(cfg_ota_t));
	if (ret < 0){
		return -1;
	}
	return 0;
}

int cfg_update_ota(cfg_ota_t *val){
	int ret = 0;
	if (!val){
		return -1;
	}
	ret = st_write_item(CFG_KEY_OTA, (uint8_t *)val, sizeof(cfg_ota_t));
	if (ret < 0){
		return -1;
	}
	return 0;
}


int cfg_init(){
	int ret = 0;
	ret = st_init();
	if (ret < 0){
		return -1;
	}
	return 0;
}

