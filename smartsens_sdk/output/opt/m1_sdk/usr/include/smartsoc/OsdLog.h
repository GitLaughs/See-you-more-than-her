#ifndef __OSD_LIB_LOG_H_
#define __OSD_LIB_LOG_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sszlog.h>

static log_rule_t cat_osd = sdk_log_get_rule((char*)"osd_rule");


#define LOGD(...)			sdk_log_debug(cat_osd, __VA_ARGS__)
#define LOGE(...)			sdk_log_error(cat_osd, __VA_ARGS__)
#define LOGI(...)			sdk_log_info(cat_osd, __VA_ARGS__)
#define LOGW(...)			sdk_log_warn(cat_osd, __VA_ARGS__)



#endif // __OSD_LIB_LOG_H_



