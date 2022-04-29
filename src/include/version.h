#ifndef __VERSION_H__
#define __VERSION_H__

// version |major   |minor    |fix       |stage|
// version |****8***|****8****|*****12****|**4**|
#define VERSION_STAGE_ALPHA	1
#define VERSION_STAGE_ALPHA_CODE	'a'
#define VERSION_STAGE_BETA	2
#define VERSION_STAGE_BETA_CODE	'b'
#define VERSION_STAGE_RELEASE	0x0fU
#define VERSION_STAGE_RELEASE_CODE	'r'



#define VERSION_GET_MAJOR(num)	((num >> 24) & 0xffUL)
#define VERSION_GET_MINOR(num)	((num >> 16) & 0xffUL)
#define VERSION_GET_FIX(num)	((num >> 4) & 0xfffUL)
#define VERSION_GET_STAGE(num) (num & 0x0fUL)
#define MK_VERSION_NUM(major, minor, fix, stage)	(((major & 0xffUL) << 24) | \
	((minor & 0xffUL) << 16) | ((fix & 0xfffUL) << 4) | (stage & 0x0fUL))

#define VERSION_MAJOR	0
#define VERSION_MINOR	0
#define VERSION_FIX		3
#define VERSION_STAGE	VERSION_STAGE_ALPHA
#define CURRENT_VERSION()	MK_VERSION_NUM(VERSION_MAJOR, VERSION_MINOR, VERSION_FIX, VERSION_STAGE)
#endif

